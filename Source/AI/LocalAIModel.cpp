#include "LocalAIModel.h"
#include <cmath>
#include <numeric>

#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

static void preloadBundledOnnxRuntime()
{
    static bool attempted = false;
    if (attempted) return;
    attempted = true;

    auto currentModule = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto moduleDir = currentModule.getParentDirectory();
    auto ortDll = moduleDir.getChildFile ("onnxruntime.dll");

    if (! ortDll.existsAsFile())
    {
        ortDll = juce::File ("C:\\Program Files\\Common Files\\VST3\\ProVerb.vst3\\Contents\\x86_64-win\\onnxruntime.dll");
    }

    if (ortDll.existsAsFile())
    {
        SetDllDirectoryW (ortDll.getParentDirectory().getFullPathName().toWideCharPointer());
        LoadLibraryExW (ortDll.getFullPathName().toWideCharPointer(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
}
#endif

LocalAIModel::LocalAIModel()
{
#if PROVERB_ENABLE_ONNX
#if JUCE_WINDOWS
    preloadBundledOnnxRuntime();
#endif
    try
    {
        env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ProVerb_LocalAI");
    }
    catch (const std::exception& e)
    {
        lastError = "Failed to initialize ONNX Runtime environment: " + juce::String (e.what());
        status = ModelStatus::LoadFailed;
    }
    catch (...)
    {
        lastError = "Unknown exception during ONNX Runtime environment initialization.";
        status = ModelStatus::LoadFailed;
    }
#else
    lastError = "ONNX Runtime support disabled at compile time.";
#endif
}

LocalAIModel::~LocalAIModel()
{
    unload();
}

void LocalAIModel::unload()
{
#if PROVERB_ENABLE_ONNX
    session.reset();
    inputName.clear();
    outputName.clear();
#endif
    status = ModelStatus::NotLoaded;
    loadedModelPath.clear();
}

juce::File LocalAIModel::locateModelFile()
{
    // 1. Directory of the loaded plugin module / binary
    auto moduleFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto moduleDir = moduleFile.getParentDirectory();

    auto candidate1 = moduleDir.getChildFile ("ProVerb_AI.onnx");
    if (candidate1.existsAsFile())
        return candidate1;

    // 2. VST3 bundle Resources folder (e.g. ProVerb.vst3/Contents/Resources/ProVerb_AI.onnx)
    auto bundleResources = moduleDir.getParentDirectory().getChildFile ("Resources").getChildFile ("ProVerb_AI.onnx");
    if (bundleResources.existsAsFile())
        return bundleResources;

    // 3. User Application Data / ProducerToy / ProVerb
    auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("ProducerToy")
                        .getChildFile ("ProVerb")
                        .getChildFile ("ProVerb_AI.onnx");
    if (appData.existsAsFile())
        return appData;

    // 4. Common Application Data (C:\ProgramData\ProducerToy\ProVerb\ProVerb_AI.onnx)
    auto commonAppData = juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                             .getChildFile ("ProducerToy")
                             .getChildFile ("ProVerb")
                             .getChildFile ("ProVerb_AI.onnx");
    if (commonAppData.existsAsFile())
        return commonAppData;

    // 5. Common VST3 directory (e.g. C:\Program Files\Common Files\VST3\ProVerb_AI.onnx)
    auto commonVst3 = juce::File ("C:\\Program Files\\Common Files\\VST3\\ProVerb_AI.onnx");
    if (commonVst3.existsAsFile())
        return commonVst3;

    // 6. Project Assets directory (development environment)
    auto currentDir = juce::File::getCurrentWorkingDirectory().getChildFile ("Assets").getChildFile ("ProVerb_AI.onnx");
    if (currentDir.existsAsFile())
        return currentDir;

    return {};
}

bool LocalAIModel::autoDiscoverAndLoad()
{
    auto file = locateModelFile();
    if (file.existsAsFile())
        return loadFromFile (file);

    lastError = "Model file 'ProVerb_AI.onnx' not found in plugin bundle or search paths.";
    status = ModelStatus::NotLoaded;
    return false;
}

bool LocalAIModel::loadFromFile (const juce::File& modelFile)
{
#if !PROVERB_ENABLE_ONNX
    juce::ignoreUnused (modelFile);
    lastError = "ONNX Runtime disabled in this build configuration.";
    status = ModelStatus::LoadFailed;
    return false;
#else
    unload();

    if (!modelFile.existsAsFile())
    {
        lastError = "Model file does not exist: " + modelFile.getFullPathName();
        status = ModelStatus::LoadFailed;
        return false;
    }

    if (!env)
    {
        try
        {
            env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ProVerb_LocalAI");
        }
        catch (const std::exception& e)
        {
            lastError = "Failed to create Ort::Env: " + juce::String (e.what());
            status = ModelStatus::LoadFailed;
            return false;
        }
    }

    try
    {
        Ort::SessionOptions sessionOptions;
        // Conservative CPU thread allocation for real-time safety
        sessionOptions.SetIntraOpNumThreads (1);
        sessionOptions.SetInterOpNumThreads (1);
        sessionOptions.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);

        const auto fullPath = modelFile.getFullPathName();
#if JUCE_WINDOWS
        session = std::make_unique<Ort::Session> (*env, fullPath.toWideCharPointer(), sessionOptions);
#else
        session = std::make_unique<Ort::Session> (*env, fullPath.toRawUTF8(), sessionOptions);
#endif

        if (!session)
        {
            lastError = "Failed to instantiate Ort::Session";
            status = ModelStatus::LoadFailed;
            return false;
        }

        // Validate model contract
        Ort::AllocatorWithDefaultOptions allocator;

        if (session->GetInputCount() != 1)
        {
            lastError = "Model input count mismatch. Expected 1 input tensor, got "
                        + juce::String (session->GetInputCount());
            unload();
            status = ModelStatus::LoadFailed;
            return false;
        }

        if (session->GetOutputCount() != 1)
        {
            lastError = "Model output count mismatch. Expected 1 output tensor, got "
                        + juce::String (session->GetOutputCount());
            unload();
            status = ModelStatus::LoadFailed;
            return false;
        }

        auto inNameAlloc = session->GetInputNameAllocated (0, allocator);
        auto outNameAlloc = session->GetOutputNameAllocated (0, allocator);
        inputName = inNameAlloc.get();
        outputName = outNameAlloc.get();

        auto inTypeInfo = session->GetInputTypeInfo (0);
        auto inTensorInfo = inTypeInfo.GetTensorTypeAndShapeInfo();
        auto inShape = inTensorInfo.GetShape();

        if (inShape.size() != 2)
        {
            lastError = "Expected 2D input tensor [batch, features], got rank " + juce::String (inShape.size());
            unload();
            status = ModelStatus::LoadFailed;
            return false;
        }

        if (inShape[1] != static_cast<int64_t> (numFeatures))
        {
            lastError = "Input feature dimension mismatch. Expected " + juce::String (numFeatures)
                        + ", model expects " + juce::String (inShape[1]);
            unload();
            status = ModelStatus::LoadFailed;
            return false;
        }

        auto outTypeInfo = session->GetOutputTypeInfo (0);
        auto outTensorInfo = outTypeInfo.GetTensorTypeAndShapeInfo();
        auto outShape = outTensorInfo.GetShape();

        if (outShape.size() != 2)
        {
            lastError = "Expected 2D output tensor [batch, classes], got rank " + juce::String (outShape.size());
            unload();
            status = ModelStatus::LoadFailed;
            return false;
        }

        if (outShape[1] != static_cast<int64_t> (numClasses))
        {
            lastError = "Output classes dimension mismatch. Expected " + juce::String (numClasses)
                        + ", model expects " + juce::String (outShape[1]);
            unload();
            status = ModelStatus::LoadFailed;
            return false;
        }

        loadedModelPath = fullPath;
        lastError.clear();
        status = ModelStatus::Loaded;
        return true;
    }
    catch (const Ort::Exception& e)
    {
        lastError = "ONNX Runtime Exception during model load: " + juce::String (e.what());
        unload();
        status = ModelStatus::LoadFailed;
        return false;
    }
    catch (const std::exception& e)
    {
        lastError = "Standard Exception during model load: " + juce::String (e.what());
        unload();
        status = ModelStatus::LoadFailed;
        return false;
    }
#endif
}

bool LocalAIModel::predict (const std::array<float, numFeatures>& features,
                            std::array<float, numClasses>& probabilities)
{
#if !PROVERB_ENABLE_ONNX
    juce::ignoreUnused (features, probabilities);
    return false;
#else
    if (status != ModelStatus::Loaded || !session)
        return false;

    try
    {
        std::array<float, numFeatures> inputValues = features;
        const std::array<int64_t, 2> inputShape { 1, static_cast<int64_t> (numFeatures) };

        auto memoryInfo = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
        auto inputTensor = Ort::Value::CreateTensor<float> (
            memoryInfo, inputValues.data(), inputValues.size(),
            inputShape.data(), inputShape.size());

        const char* inputNames[]  = { inputName.c_str() };
        const char* outputNames[] = { outputName.c_str() };

        auto outputs = session->Run (
            Ort::RunOptions { nullptr },
            inputNames, &inputTensor, 1,
            outputNames, 1);

        if (outputs.empty() || !outputs[0].IsTensor())
        {
            lastError = "ONNX session returned invalid output tensor";
            return false;
        }

        auto outputInfo = outputs[0].GetTensorTypeAndShapeInfo();
        if (outputInfo.GetElementCount() != static_cast<int64_t> (numClasses))
        {
            lastError = "Output tensor element count mismatch";
            return false;
        }

        const float* rawOutput = outputs[0].GetTensorData<float>();

        // Check if output is raw logits (sum not close to 1.0 or any value < 0)
        float sum = 0.0f;
        bool hasNegative = false;
        for (std::size_t i = 0; i < numClasses; ++i)
        {
            sum += rawOutput[i];
            if (rawOutput[i] < 0.0f)
                hasNegative = true;
        }

        if (hasNegative || std::abs (sum - 1.0f) > 0.05f)
        {
            // Apply Softmax numerically stable: exp(x_i - max) / sum(exp)
            float maxVal = rawOutput[0];
            for (std::size_t i = 1; i < numClasses; ++i)
                maxVal = std::max (maxVal, rawOutput[i]);

            float expSum = 0.0f;
            for (std::size_t i = 0; i < numClasses; ++i)
            {
                probabilities[i] = std::exp (rawOutput[i] - maxVal);
                expSum += probabilities[i];
            }

            if (expSum > 1.0e-9f)
            {
                for (std::size_t i = 0; i < numClasses; ++i)
                    probabilities[i] /= expSum;
            }
        }
        else
        {
            std::copy_n (rawOutput, numClasses, probabilities.begin());
        }

        return true;
    }
    catch (const Ort::Exception& e)
    {
        lastError = "ONNX Runtime inference error: " + juce::String (e.what());
        return false;
    }
    catch (const std::exception& e)
    {
        lastError = "Inference error: " + juce::String (e.what());
        return false;
    }
#endif
}
