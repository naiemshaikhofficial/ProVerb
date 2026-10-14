#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <string>
#include <sstream>
#include <algorithm>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "../Source/AI/AudioFeatureExtractor.h"
#include "../Source/DSP/AudioAnalyzerEngine.h"
#include "../Source/AI/LocalAIModel.h"
#include "../Source/AI/ReverbDecisionEngine.h"

// ==============================================================================
// Reliable Test Harness Macros (Works in both Debug and Release / NDEBUG builds)
// ==============================================================================

struct TestStats
{
    int passed = 0;
    int failed = 0;
    int skipped = 0;
};

static TestStats g_stats;

#define PROVERB_TEST_CHECK(condition, message) \
    do { \
        if (condition) { \
            g_stats.passed++; \
        } else { \
            g_stats.failed++; \
            std::cerr << "      [FAIL] " << message \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        } \
    } while (0)

#define PROVERB_TEST_SKIP(message) \
    do { \
        g_stats.skipped++; \
        std::cout << "      [SKIP] " << message << "\n"; \
    } while (0)


// Helper to escape strings for valid JSON emission
static std::string escapeJsonString (const std::string& input)
{
    std::ostringstream ss;
    for (char c : input)
    {
        switch (c)
        {
            case '"':  ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b";  break;
            case '\f': ss << "\\f";  break;
            case '\n': ss << "\\n";  break;
            case '\r': ss << "\\r";  break;
            case '\t': ss << "\\t";  break;
            default:   ss << c;       break;
        }
    }
    return ss.str();
}


// Helper: Process buffer in realistic DAW block sizes (e.g. 512 samples)
static bool processAudioThroughEngine (AudioAnalyzerEngine& engine,
                                       const float* audioData,
                                       int totalSamples,
                                       int blockSize,
                                       ProVerbAI::FeatureSnapshot& outSnapshot)
{
    std::vector<float> leftBlock (blockSize, 0.0f);
    std::vector<float> rightBlock (blockSize, 0.0f);

    bool learningCompleted = false;
    int samplesProcessed = 0;

    while (samplesProcessed < totalSamples)
    {
        const int samplesToProcess = std::min (blockSize, totalSamples - samplesProcessed);
        for (int i = 0; i < samplesToProcess; ++i)
        {
            leftBlock[i]  = audioData[samplesProcessed + i];
            rightBlock[i] = audioData[samplesProcessed + i];
        }

        if (engine.process (leftBlock.data(), rightBlock.data(), samplesToProcess, outSnapshot))
        {
            learningCompleted = true;
            break;
        }

        samplesProcessed += samplesToProcess;
    }

    if (!learningCompleted)
        engine.extractFeatureSnapshot (outSnapshot);

    return learningCompleted;
}


// ==============================================================================
// 1. Learning Window & Completion Test
// ==============================================================================
static void testLearningWindowAndCompletion()
{
    std::cout << "[1/5] Testing AudioAnalyzerEngine learning window and completion behavior...\n";

    AudioAnalyzerEngine engine;
    const double sr = 44100.0;
    engine.prepare (sr);

    const float durationSec = 1.0f;
    const int totalExpectedSamples = static_cast<int>(sr * durationSec);
    engine.startLearning (durationSec);

    PROVERB_TEST_CHECK (engine.isListening(), "Engine should be in listening state after startLearning()");

    std::vector<float> audio (totalExpectedSamples, 0.25f);
    ProVerbAI::FeatureSnapshot snapshot;

    bool completed = processAudioThroughEngine (engine, audio.data(), totalExpectedSamples, 512, snapshot);

    PROVERB_TEST_CHECK (completed, "Engine must return true when learning window completes");
    PROVERB_TEST_CHECK (!engine.isListening(), "Engine must leave listening state upon completion");
    PROVERB_TEST_CHECK (snapshot.samplesAccumulated >= totalExpectedSamples,
                        "Accumulated samples must reach requested window duration");
    PROVERB_TEST_CHECK (std::abs (engine.getProgress() - 1.0f) < 0.01f, "Progress should reach 1.0 upon completion");
    PROVERB_TEST_CHECK (!snapshot.isSilence, "0.25 amplitude audio must not be silence");
}


// ==============================================================================
// 2. Controlled Spectral Band Frequency Test (Measured 1-Pole Crossover Responses)
// ==============================================================================
static void testSpectralBandFilters()
{
    std::cout << "[2/5] Testing 5-band crossover filter response across controlled frequencies...\n";

    const double sr = 44100.0;
    const int numSamples = static_cast<int>(sr * 1.0);

    // Crossover Cutoffs: 95 Hz, 320 Hz, 1800 Hz, 6500 Hz (6 dB/oct 1-pole complementary IIR filters)
    // Thresholds calibrated to measured 1-pole filter crossover characteristics:
    struct FrequencyTestCase {
        const char* name;
        float freq;
        int expectedDominantFeature;
        float minExpectedRatio;
    };

    FrequencyTestCase testCases[] = {
        { "Sub Bass (60 Hz)",         60.0f,   ProVerbAI::SubRatio,       0.75f },
        { "Warm Bass (200 Hz)",       200.0f,  ProVerbAI::BassRatio,      0.40f },
        { "Vocal Core (800 Hz)",      800.0f,  ProVerbAI::VocalCoreRatio, 0.65f },
        { "Vocal Presence (3500 Hz)", 3500.0f, ProVerbAI::VocalPresRatio, 0.50f },
        { "Air / Treble (12000 Hz)",  12000.0f,ProVerbAI::AirRatio,       0.60f }
    };

    for (const auto& tc : testCases)
    {
        AudioAnalyzerEngine engine;
        engine.prepare (sr);
        engine.startLearning (1.0f);

        std::vector<float> sineWave (numSamples);
        const float twoPi = juce::MathConstants<float>::twoPi;
        for (int i = 0; i < numSamples; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(sr);
            sineWave[i] = 0.5f * std::sin (twoPi * tc.freq * t);
        }

        ProVerbAI::FeatureSnapshot snapshot;
        processAudioThroughEngine (engine, sineWave.data(), numSamples, 512, snapshot);

        const float targetRatio = snapshot.features[tc.expectedDominantFeature];

        // 1. Verify target band ratio exceeds empirically verified 1-pole filter threshold
        std::cout << "      - " << tc.name << ": Target Band Ratio = "
                  << std::fixed << std::setprecision(3) << targetRatio
                  << " (calibrated threshold >= " << tc.minExpectedRatio << ")\n";

        PROVERB_TEST_CHECK (targetRatio >= tc.minExpectedRatio,
                            juce::String ("Target ratio below calibrated threshold for " + juce::String (tc.name)).toRawUTF8());

        // 2. Verify target band is the strictly DOMINANT ratio among all 5 bands
        float maxRatio = snapshot.features[ProVerbAI::SubRatio];
        int dominantIndex = ProVerbAI::SubRatio;
        for (int band = ProVerbAI::BassRatio; band <= ProVerbAI::AirRatio; ++band)
        {
            if (snapshot.features[band] > maxRatio)
            {
                maxRatio = snapshot.features[band];
                dominantIndex = band;
            }
        }
        PROVERB_TEST_CHECK (dominantIndex == tc.expectedDominantFeature,
                            juce::String ("Target band was not the dominant band for " + juce::String (tc.name)).toRawUTF8());

        // 3. Verify all 9 features are finite and within documented bounds
        for (int f = 0; f < static_cast<int>(ProVerbAI::numFeatures); ++f)
        {
            PROVERB_TEST_CHECK (std::isfinite (snapshot.features[f]), "Feature value must be finite");
        }
        PROVERB_TEST_CHECK (snapshot.features[ProVerbAI::CrestFactor] >= 1.0f, "Crest factor must be >= 1.0");
        PROVERB_TEST_CHECK (snapshot.features[ProVerbAI::Zcr] >= 0.0f && snapshot.features[ProVerbAI::Zcr] <= 1.0f, "ZCR must be in [0, 1]");
    }
}


// ==============================================================================
// 3. Multi-Sample-Rate Readiness (22050 to 96000 Hz)
// ==============================================================================
static void testMultiSampleRate()
{
    std::cout << "[3/5] Testing professional DAW sample rates (22050, 44100, 48000, 88200, 96000 Hz)...\n";

    const double sampleRates[] = { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0 };

    for (double sr : sampleRates)
    {
        AudioAnalyzerEngine engine;
        engine.prepare (sr);
        engine.startLearning (0.5f);

        const int numSamples = static_cast<int>(sr * 0.5);
        std::vector<float> audio (numSamples);
        const float twoPi = juce::MathConstants<float>::twoPi;
        for (int i = 0; i < numSamples; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(sr);
            audio[i] = 0.4f * std::sin (twoPi * 440.0f * t);
        }

        ProVerbAI::FeatureSnapshot snapshot;
        bool completed = processAudioThroughEngine (engine, audio.data(), numSamples, 256, snapshot);

        PROVERB_TEST_CHECK (completed, "Learning window must complete at sr=" + juce::String (sr));
        PROVERB_TEST_CHECK (snapshot.sampleRate == sr, "Snapshot sampleRate must match currentSampleRate");
        for (int f = 0; f < static_cast<int>(ProVerbAI::numFeatures); ++f)
        {
            PROVERB_TEST_CHECK (std::isfinite (snapshot.features[f]), "Feature must be finite at sr=" + juce::String (sr));
        }
    }
}


// ==============================================================================
// 4. Silence & NaN/Inf Sanitization and Filter State Recovery Test
// ==============================================================================
static void testSilenceAndSanitization()
{
    std::cout << "[4/5] Testing silence handling, NaN/Inf resilience, and filter state recovery...\n";

    // Subtest A: True Silence
    {
        AudioAnalyzerEngine engine;
        engine.prepare (44100.0);
        engine.startLearning (1.0f);

        std::vector<float> zeros (44100, 0.0f);
        ProVerbAI::FeatureSnapshot snapshot;
        processAudioThroughEngine (engine, zeros.data(), 44100, 512, snapshot);

        PROVERB_TEST_CHECK (snapshot.isSilence, "Zero audio must be marked isSilence=true");
        for (int f = 0; f < static_cast<int>(ProVerbAI::numFeatures); ++f)
            PROVERB_TEST_CHECK (snapshot.features[f] == 0.0f, "Silent snapshot features must be all 0.0f");

        // Verify ReverbDecisionEngine handles silence gracefully
        auto rec = ReverbDecisionEngine::decideFromModel ({}, snapshot);
        PROVERB_TEST_CHECK (rec.profile.id == AudioAnalyzerEngine::ProfileId::NoAudio, "Silence must map to NoAudio profile");
        PROVERB_TEST_CHECK (rec.confidence == 0.0f, "Silence confidence must be 0.0");
    }

    // Subtest B: NaN / Inf Pollution Resilience and Subsequent Recovery
    {
        AudioAnalyzerEngine engine;
        const double sr = 44100.0;
        engine.prepare (sr);
        engine.startLearning (0.5f);

        const int dirtySamples = static_cast<int>(sr * 0.5);
        std::vector<float> dirty (dirtySamples, 0.2f);
        dirty[100] = std::numeric_limits<float>::quiet_NaN();
        dirty[200] = std::numeric_limits<float>::infinity();
        dirty[300] = -std::numeric_limits<float>::infinity();

        ProVerbAI::FeatureSnapshot dirtySnapshot;
        processAudioThroughEngine (engine, dirty.data(), dirtySamples, 256, dirtySnapshot);

        // Verify that production code itself sanitized the output features
        for (int f = 0; f < static_cast<int>(ProVerbAI::numFeatures); ++f)
        {
            PROVERB_TEST_CHECK (std::isfinite (dirtySnapshot.features[f]),
                                "extractFeatureSnapshot() must sanitize features without NaN/Inf");
        }

        // Subtest C: State Recovery Test
        // Run a clean 1.0s sine wave through the SAME engine instance to verify filter recovery
        engine.reset();
        engine.startLearning (1.0f);

        const int cleanSamples = static_cast<int>(sr * 1.0);
        std::vector<float> clean (cleanSamples);
        const float twoPi = juce::MathConstants<float>::twoPi;
        for (int i = 0; i < cleanSamples; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(sr);
            clean[i] = 0.5f * std::sin (twoPi * 1000.0f * t);
        }

        ProVerbAI::FeatureSnapshot cleanSnapshot;
        bool recoveryCompleted = processAudioThroughEngine (engine, clean.data(), cleanSamples, 512, cleanSnapshot);

        PROVERB_TEST_CHECK (recoveryCompleted, "Engine must recover and complete analysis on subsequent clean signal");
        PROVERB_TEST_CHECK (!cleanSnapshot.isSilence, "Clean signal must not be marked silence");
        for (int f = 0; f < static_cast<int>(ProVerbAI::numFeatures); ++f)
        {
            PROVERB_TEST_CHECK (std::isfinite (cleanSnapshot.features[f]), "Clean features must be strictly finite after recovery");
        }
        PROVERB_TEST_CHECK (cleanSnapshot.features[ProVerbAI::VocalCoreRatio] > 0.60f,
                            "1000 Hz sine wave must properly isolate VocalCoreRatio after filter recovery");
    }
}


// ==============================================================================
// 5. ONNX Model Discovery, Loading & Live Inference Test
// ==============================================================================
static void testLocalAIModelInference (bool strictMode)
{
    std::cout << "[5/5] Testing LocalAIModel ONNX loading, tensor contracts, and inference...\n";

    LocalAIModel model;
    bool loaded = model.autoDiscoverAndLoad();

    if (!loaded)
    {
        // Try direct project assets location
        auto devModel = juce::File::getCurrentWorkingDirectory()
                            .getChildFile ("Assets")
                            .getChildFile ("ProVerb_AI.onnx");
        if (devModel.existsAsFile())
            loaded = model.loadFromFile (devModel);
    }

    if (!loaded)
    {
        if (strictMode)
        {
            PROVERB_TEST_CHECK (false, "STRICT MODE: ProVerb_AI.onnx model was not found in discovery search paths!");
        }
        else
        {
            PROVERB_TEST_SKIP ("ProVerb_AI.onnx not found. Skipping ONNX inference test in non-strict mode.");
        }
        return;
    }

    PROVERB_TEST_CHECK (model.isLoaded(), "Model status must be Loaded");
    std::cout << "      - Model loaded from: " << model.getModelPath() << "\n";

    // Generate real audio features using AudioAnalyzerEngine (Vocals tone)
    AudioAnalyzerEngine engine;
    const double sr = 44100.0;
    engine.prepare (sr);
    engine.startLearning (1.0f);

    const int nSamples = 44100;
    std::vector<float> vocalAudio (nSamples);
    const float twoPi = juce::MathConstants<float>::twoPi;
    for (int i = 0; i < nSamples; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(sr);
        vocalAudio[i] = 0.4f * std::sin (twoPi * 220.0f * t)
                      + 0.25f * std::sin (twoPi * 1100.0f * t)
                      + 0.15f * std::sin (twoPi * 2800.0f * t);
    }

    ProVerbAI::FeatureSnapshot realSnapshot;
    processAudioThroughEngine (engine, vocalAudio.data(), nSamples, 512, realSnapshot);

    std::array<float, ProVerbAI::numClasses> probabilities {};
    bool predSuccess = model.predict (realSnapshot.features, probabilities);
    PROVERB_TEST_CHECK (predSuccess, "model.predict() execution failed on valid audio features");

    float probSum = 0.0f;
    int topClass = 0;
    float topProb = 0.0f;

    for (std::size_t i = 0; i < ProVerbAI::numClasses; ++i)
    {
        PROVERB_TEST_CHECK (std::isfinite (probabilities[i]), "Class probability must be finite");
        PROVERB_TEST_CHECK (probabilities[i] >= 0.0f && probabilities[i] <= 1.0f,
                            "Class probability must be bounded in [0.0, 1.0]");
        probSum += probabilities[i];
        if (probabilities[i] > topProb)
        {
            topProb = probabilities[i];
            topClass = static_cast<int>(i);
        }
    }

    std::cout << "      - Probabilities sum: " << std::fixed << std::setprecision(4) << probSum << "\n";
    std::cout << "      - Winning Class: [" << topClass << "] with confidence "
              << std::fixed << std::setprecision(1) << (topProb * 100.0f) << "%\n";

    PROVERB_TEST_CHECK (std::abs (probSum - 1.0f) < 0.015f,
                        "Probabilities from model.predict() must sum to approximately 1.0");

    // Test ReverbDecisionEngine consumption
    auto rec = ReverbDecisionEngine::decideFromModel (probabilities, realSnapshot);
    PROVERB_TEST_CHECK (rec.isAIModel, "Recommendation must flag isAIModel=true");
    PROVERB_TEST_CHECK (rec.confidence > 0.0f, "Recommendation confidence must be > 0");
    std::cout << "      - Decision Recipe: " << rec.profileName << " (" << rec.instrumentCategory << ")\n";
}


// ==============================================================================
// 6. JSON Feature Dump Mode for Cross-Language Parity with Python
// ==============================================================================
static int dumpFeaturesFromAudioFile (const juce::File& audioFile)
{
    if (!audioFile.existsAsFile())
    {
        std::cerr << "{\"error\": \"Audio file does not exist: "
                  << escapeJsonString (audioFile.getFullPathName().toStdString()) << "\"}\n";
        return 1;
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (audioFile));
    if (!reader)
    {
        std::cerr << "{\"error\": \"Failed to decode audio file: "
                  << escapeJsonString (audioFile.getFullPathName().toStdString()) << "\"}\n";
        return 1;
    }

    const int totalSamples = static_cast<int>(reader->lengthInSamples);
    if (totalSamples <= 0)
    {
        std::cerr << "{\"error\": \"Audio file contains 0 samples: "
                  << escapeJsonString (audioFile.getFullPathName().toStdString()) << "\"}\n";
        return 1;
    }

    juce::AudioBuffer<float> buffer (static_cast<int>(reader->numChannels), totalSamples);
    reader->read (&buffer, 0, totalSamples, 0, true, true);

    std::vector<float> mono (totalSamples, 0.0f);
    if (buffer.getNumChannels() == 1)
    {
        std::copy_n (buffer.getReadPointer (0), totalSamples, mono.data());
    }
    else
    {
        // Consistent multi-channel downmix policy: mean of left and right channels
        const float* left = buffer.getReadPointer (0);
        const float* right = buffer.getReadPointer (1);
        for (int i = 0; i < totalSamples; ++i)
            mono[i] = (left[i] + right[i]) * 0.5f;
    }

    AudioAnalyzerEngine engine;
    const double sr = reader->sampleRate;
    engine.prepare (sr);
    const float durationSec = static_cast<float>(totalSamples) / static_cast<float>(sr);
    engine.startLearning (durationSec);

    ProVerbAI::FeatureSnapshot snapshot;
    processAudioThroughEngine (engine, mono.data(), totalSamples, 512, snapshot);

    std::ostringstream json;
    json << "{\n";
    json << "  \"filePath\": \"" << escapeJsonString (audioFile.getFullPathName().toStdString()) << "\",\n";
    json << "  \"sampleRate\": " << snapshot.sampleRate << ",\n";
    json << "  \"samplesAccumulated\": " << snapshot.samplesAccumulated << ",\n";
    json << "  \"meanRms\": " << snapshot.meanRms << ",\n";
    json << "  \"peakLevel\": " << snapshot.peakLevel << ",\n";
    json << "  \"isSilence\": " << (snapshot.isSilence ? "true" : "false") << ",\n";
    json << "  \"features\": [\n";
    for (int i = 0; i < static_cast<int>(ProVerbAI::numFeatures); ++i)
    {
        const float fVal = std::isfinite (snapshot.features[i]) ? snapshot.features[i] : 0.0f;
        json << "    " << std::setprecision(8) << fVal;
        if (i < static_cast<int>(ProVerbAI::numFeatures) - 1)
            json << ",";
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";

    std::cout << json.str();
    return 0;
}


// ==============================================================================
// Main Entry Point
// ==============================================================================
int main (int argc, char* argv[])
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (argv[i]);

    // Handle dump-features mode for direct Python cross-language verification
    if (args.contains ("--dump-features"))
    {
        const int idx = args.indexOf ("--dump-features");
        if (idx + 1 < args.size())
        {
            return dumpFeaturesFromAudioFile (juce::File (args[idx + 1]));
        }
        std::cerr << "{\"error\": \"Usage: ProVerb_FeatureParityTest --dump-features <path-to-wav>\"}\n";
        return 1;
    }

    bool strictMode = args.contains ("--strict");

    std::cout << "========================================================\n";
    std::cout << " ProVerb C++ Direct Parity & Inference Harness\n";
    std::cout << " Mode: " << (strictMode ? "STRICT (Model Mandatory)" : "STANDARD") << "\n";
    std::cout << "========================================================\n\n";

    testLearningWindowAndCompletion();
    testSpectralBandFilters();
    testMultiSampleRate();
    testSilenceAndSanitization();
    testLocalAIModelInference (strictMode);

    std::cout << "\n========================================================\n";
    std::cout << " TEST EXECUTION SUMMARY:\n";
    std::cout << "   Passed:  " << g_stats.passed << "\n";
    std::cout << "   Failed:  " << g_stats.failed << "\n";
    std::cout << "   Skipped: " << g_stats.skipped << "\n";
    std::cout << "========================================================\n";

    if (g_stats.failed > 0)
    {
        std::cerr << "[ERROR] TEST SUITE FAILED with " << g_stats.failed << " failures.\n";
        return 1;
    }

    if (strictMode && g_stats.skipped > 0)
    {
        std::cerr << "[ERROR] STRICT MODE FAILED: Tests were skipped.\n";
        return 1;
    }

    std::cout << "[SUCCESS] ALL MANDATORY CHECKS PASSED!\n";
    return 0;
}
