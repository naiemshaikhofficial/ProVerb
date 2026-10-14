#pragma once

#include <juce_core/juce_core.h>
#include "AudioFeatureExtractor.h"
#include <array>
#include <memory>
#include <string>

#ifndef PROVERB_ENABLE_ONNX
#define PROVERB_ENABLE_ONNX 1
#endif

#if PROVERB_ENABLE_ONNX
#ifndef ORT_API_VERSION
#define ORT_API_VERSION 17
#endif
#include <onnxruntime_cxx_api.h>
#endif

/**
 * LocalAIModel
 * 
 * Offline, CPU-only ONNX Runtime C++ wrapper for ProVerb.
 * 
 * Execution rules:
 * - NEVER called from the real-time audio thread.
 * - Called strictly from the background inference worker thread.
 * - Conservative thread count (1 thread) to prevent DAW audio glitches.
 * - Validates model tensor shape [1, 9] -> [1, 7] and Float32 type.
 * - Completely local: zero network, zero HTTP, zero cloud dependencies.
 */
class LocalAIModel
{
public:
    static constexpr std::size_t numFeatures = ProVerbAI::numFeatures;
    static constexpr std::size_t numClasses  = ProVerbAI::numClasses;

    enum class ModelStatus
    {
        NotLoaded = 0,
        Loaded = 1,
        LoadFailed = 2
    };

    LocalAIModel();
    ~LocalAIModel();

    // Call from worker or initialization thread (NEVER audio callback)
    bool loadFromFile (const juce::File& modelFile);
    bool autoDiscoverAndLoad();
    void unload();

    // Call from background worker thread (NEVER audio callback)
    bool predict (const std::array<float, numFeatures>& features,
                  std::array<float, numClasses>& probabilities);

    bool isLoaded() const noexcept { return status == ModelStatus::Loaded; }
    ModelStatus getStatus() const noexcept { return status; }
    juce::String getLastError() const { return lastError; }
    juce::String getModelPath() const { return loadedModelPath; }

    static juce::File locateModelFile();

private:
    ModelStatus status { ModelStatus::NotLoaded };
    juce::String loadedModelPath;
    juce::String lastError;

#if PROVERB_ENABLE_ONNX
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    std::string inputName;
    std::string outputName;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LocalAIModel)
};
