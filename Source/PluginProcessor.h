#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/FDNReverb.h"
#include "DSP/DuckingEngine.h"
#include "DSP/ShimmerEngine.h"
#include "DSP/ToneEQ.h"
#include "DSP/AudioAnalyzerEngine.h"
#include "AI/AIInferenceWorker.h"
#include "AI/SourceDetector.h"

class ProVerbAudioProcessor : public juce::AudioProcessor,
                              public juce::AsyncUpdater
{
public:
    ProVerbAudioProcessor();
    ~ProVerbAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    struct Preset
    {
        juce::String category;
        juce::String name;
        float predelayMs;
        float size;
        float decaySeconds;
        float diffusion;
        float damping;
        float lowCut;
        float highCut;
        float ducking;
        float shimmer;
        float width;
        float wet;
        float freeze;
        float dry = 100.0f;
        float inputGain = 0.0f;
        float outputGain = 0.0f;
        float midSide = 0.0f;
        float eqLowShelfFreq = 250.0f;
        float eqLowShelfGain = 0.0f;
        float eqMidFreq = 1200.0f;
        float eqMidGain = 0.0f;
        float eqMidQ = 0.70f;
        float eqHighShelfFreq = 5000.0f;
        float eqHighShelfGain = 0.0f;
        float algoMode = 0.0f;
        float toneColor = 0.0f;
        float modRate = 1.0f;
        float modDepth = 40.0f;
        float bassDecay = 1.0f;
        float bassCrossover = 350.0f;
        float earlySize = 70.0f;
        float earlyCross = 50.0f;
        float earlySend = 20.0f;
        float predelaySync = 0.0f;
        juce::String aliases = {};
        juce::String description = {};
    };

    static const std::vector<Preset>& getPresetList();
    void loadPreset (int index);

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    std::function<void(int)> onProgramChange;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Smart AI Audio Analyzer (Auto Learn)
    void triggerAutoLearn();
    void abortAutoLearn();
    bool isAutoLearning() const { return isAnalyzingActive.load (std::memory_order_acquire) || analyzerEngine.isListening(); }
    float getAutoLearnProgress() const { return analyzerEngine.getProgress(); }
    bool hasAutoLearned() const { return hasAnalyzedResult; }
    juce::String getAutoLearnProfileName() const { return autoLearnProfileName; }
    juce::String getDetectedInstrument() const { return detectedInstrument; }
    juce::String getDetectedPresetName() const { return detectedPresetName; }
    juce::String getDetectedProfileName() const { return detectedPresetName; }
    AudioAnalyzerEngine::ProfileId getDetectedProfileId() const { return lastProfileId; }

    static bool isDevMode() noexcept
    {
#if defined(DEBUG) || defined(_DEBUG) || JUCE_DEBUG || PROVERB_DEV_MODE
        return true;
#else
        return juce::SystemStats::getEnvironmentVariable ("PROVERB_DEV", juce::String()).isNotEmpty();
#endif
    }

    juce::String getAIPresetDisplayName() const
    {
        if (isDevMode())
            return "AI: " + detectedPresetName;
        return "Smart Space";
    }

    bool canUndoAutoLearn() const noexcept { return hasUndoState; }
    void undoAutoLearn();
    void analyzeAudioFileAsync (const juce::File& audioFile);

    std::function<void (const juce::String& instrument, const juce::String& profileName, bool success)> onAutoLearnResult;

    // Offline ONNX AI Inference Status
    bool isAIModelLoaded() const noexcept { return aiWorker.isModelLoaded(); }
    juce::String getAIModelStatusText() const { return aiWorker.getModelStatusText(); }
    AIInferenceWorker& getAIWorker() noexcept { return aiWorker; }

    // Multi-Label Real-Time AI Source Detector
    int getDetectedSourceFamily() const noexcept { return sourceDetector.getDominantFamily(); }
    float getDetectedSourceConfidence() const noexcept { return sourceDetector.getDominantConfidence(); }
    float getSourceFamilyProbability (int familyIndex) const noexcept { return sourceDetector.getFamilyProbability (familyIndex); }
    juce::String getDetectedSourceFamilyName() const { return SourceDetector::getFamilyName (sourceDetector.getDominantFamily()); }
    juce::String getDetectedSourceAliases() const { return SourceDetector::getFamilyAliases (sourceDetector.getDominantFamily()); }
    bool isSourceFamilyActive (int familyIndex) const noexcept { return sourceDetector.isFamilyActive (familyIndex); }
    uint32_t getActiveSourceFamiliesMask() const noexcept { return sourceDetector.getActiveFamiliesMask(); }
    int getActiveSourceFamilyCount() const noexcept { return sourceDetector.getActiveFamilyCount(); }
    juce::String getActiveSourceFamiliesSummary() const { return sourceDetector.getActiveFamiliesSummary(); }
    juce::String getDetectedSourceRecommendedPreset() const
    {
        auto* p = SourceDetector::getRecommendedPresetName (sourceDetector.getDominantFamily());
        return p ? juce::String (p) : juce::String();
    }
    void setSourceDetectorEnabled (bool enabled) noexcept { sourceDetector.setEnabled (enabled); }
    bool isSourceDetectorEnabled() const noexcept { return sourceDetector.isEnabled(); }
    void applyDetectedSourcePreset();

    // Atomic meters for thread-safe UI updates
    float getRmsInput() const { return rmsInput.load(); }
    float getRmsOutput() const { return rmsOutput.load(); }
    float getRmsSidechain() const noexcept { return rmsSidechain.load (std::memory_order_relaxed); }
    bool isSidechainActive() const noexcept { return isSidechainSignalActive.load (std::memory_order_relaxed); }
    bool hasSidechainConnection() const noexcept { return isSidechainBusConnected.load (std::memory_order_relaxed); }
    float getEffectiveSmartSpace() const noexcept { return currentSmartSpace.load (std::memory_order_relaxed); }
    float getEffectiveSmartClarity() const noexcept { return currentSmartClarity.load (std::memory_order_relaxed); }
    float getStereoPhaseCorrelation() const noexcept { return stereoPhaseCorrelation.load (std::memory_order_relaxed); }
    float getDuckingGainReductionDb() const noexcept { return duckingEngine.getGainReductionDb(); }

    // Live 3D Audio-Reactive Prism Telemetry Contract
    struct PrismTelemetry
    {
        float inputRms { 0.0f };
        float inputPeak { 0.0f };
        float outputRms { 0.0f };
        float wetRms { 0.0f };
        float bassEnergy { 0.0f };
        float midEnergy { 0.0f };
        float trebleEnergy { 0.0f };
        float transientScore { 0.0f };
        float phaseCorr { 1.0f };
        float duckingDb { 0.0f };
    };

    PrismTelemetry getPrismTelemetry() const noexcept
    {
        PrismTelemetry t;
        t.inputRms       = rmsInput.load (std::memory_order_relaxed);
        t.inputPeak      = prismInputPeak.load (std::memory_order_relaxed);
        t.outputRms      = rmsOutput.load (std::memory_order_relaxed);
        t.wetRms         = prismWetRms.load (std::memory_order_relaxed);
        t.bassEnergy     = prismBassEnergy.load (std::memory_order_relaxed);
        t.midEnergy      = prismMidEnergy.load (std::memory_order_relaxed);
        t.trebleEnergy   = prismTrebleEnergy.load (std::memory_order_relaxed);
        t.transientScore = prismTransientScore.load (std::memory_order_relaxed);
        t.phaseCorr      = stereoPhaseCorrelation.load (std::memory_order_relaxed);
        t.duckingDb      = duckingEngine.getGainReductionDb();
        return t;
    }

    // Fast, zero-allocation license checks for audio and GUI threads
    bool isLicenseValid() const noexcept { return isLicensedAtomic.load (std::memory_order_relaxed); }
    uint64_t getEntitlementProof() const noexcept { return entitlementProofAtomic.load (std::memory_order_relaxed); }
    void updateLicenseStatus() noexcept;

private:
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState apvts;

    // DSP Engines
    FDNReverb fdnReverb;
    DuckingEngine duckingEngine;
    ShimmerEngine shimmerEngine;
    ToneEQ toneEQ;
    AudioAnalyzerEngine analyzerEngine;
    SourceDetector sourceDetector;

    // Local Offline AI Worker
    AIInferenceWorker aiWorker;
    ReverbDecisionEngine::Recommendation pendingRecommendation;

    AudioAnalyzerEngine::AcousticProfile pendingProfile;
    juce::String autoLearnProfileName;
    bool hasAnalyzedResult { false };
    AudioAnalyzerEngine::ProfileId lastProfileId { AudioAnalyzerEngine::ProfileId::None };
    juce::String detectedInstrument;
    juce::String detectedPresetName;
    std::atomic<bool> isAnalyzingActive { false };

    // Auto Learn Undo / Suggestion State
    bool hasUndoState { false };
    bool hasPendingSuggestion { false };
    int suggestedProgram { -1 };
    std::map<juce::String, float> stateBeforeAutoLearn;
    int programBeforeAutoLearn { 0 };
    float suggestedConfidence { 0.0f };

    // Pre-delay line & smoothing
    juce::dsp::DelayLine<float> preDelayLine { 96000 };
    juce::SmoothedValue<float> smoothedPreDelay;

    // Smoothed gain parameters
    juce::SmoothedValue<float> smoothedInputGain;
    juce::SmoothedValue<float> smoothedOutputGain;
    juce::SmoothedValue<float> smoothedWetGain;
    juce::SmoothedValue<float> smoothedDryGain;
    juce::SmoothedValue<float> smoothedBypassGain;
    juce::SmoothedValue<float> smoothedWidth;

    // Smoothed Next-Level AI Macro Automation
    juce::SmoothedValue<float> smoothedSmartSpace;
    juce::SmoothedValue<float> smoothedSmartClarity;
    juce::SmoothedValue<float> smoothedSmartPumping;

    // Scratch buffers for stereo processing
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> immediateBuffer;

    // Thread-safe meter levels
    std::atomic<float> rmsInput  { 0.0f };
    std::atomic<float> rmsOutput { 0.0f };
    std::atomic<float> rmsSidechain { 0.0f };
    std::atomic<bool> isSidechainSignalActive { false };
    std::atomic<bool> isSidechainBusConnected { false };
    std::atomic<float> currentSmartSpace { 50.0f };
    std::atomic<float> currentSmartClarity { 50.0f };
    std::atomic<float> stereoPhaseCorrelation { 1.0f };

    // Real-Time 3D Audio-Reactive Prism telemetry atomics & audio-thread filter states
    std::atomic<float> prismInputPeak { 0.0f };
    std::atomic<float> prismWetRms { 0.0f };
    std::atomic<float> prismBassEnergy { 0.0f };
    std::atomic<float> prismMidEnergy { 0.0f };
    std::atomic<float> prismTrebleEnergy { 0.0f };
    std::atomic<float> prismTransientScore { 0.0f };
    float prismLpBass { 0.0f };
    float prismLpMid { 0.0f };
    float prismFastEnv { 0.0f };
    float prismSlowEnv { 0.0f };

    int currentProgram { 0 };

    // Real-Time Audio-Thread Safe Cached License State (Zero Allocations, Zero File I/O)
    std::atomic<bool> isLicensedAtomic { false };
    std::atomic<uint64_t> entitlementProofAtomic { 0ULL };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProVerbAudioProcessor)
};
