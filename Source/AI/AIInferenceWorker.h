#pragma once

#include <juce_core/juce_core.h>
#include "LocalAIModel.h"
#include "ReverbDecisionEngine.h"
#include "AudioFeatureExtractor.h"
#include <atomic>
#include <functional>

/**
 * AIInferenceWorker
 * 
 * Dedicated background worker thread for ProVerb's AI Engine.
 * 
 * Guarantees:
 * - Real-time audio thread NEVER calls ONNX inference or mutex locks.
 * - Lock-free, wait-free job handoff from audio thread using atomics and WaitableEvent.
 * - Safe lifecycle management ensuring no race conditions during DAW closing or unmount.
 */
class AIInferenceWorker : private juce::Thread
{
public:
    AIInferenceWorker();
    ~AIInferenceWorker() override;

    void startWorker();
    void stopWorker();

    // Call from background or message thread
    bool loadModel (const juce::File& file);
    bool autoDiscoverModel();
    bool isModelLoaded() const noexcept { return model.isLoaded(); }
    juce::String getModelStatusText() const;

    // Called strictly from audio callback (Wait-free, zero allocation)
    bool postAnalysisJob (const ProVerbAI::FeatureSnapshot& snapshot) noexcept;

    // Set callback to receive recommendations on worker thread
    std::function<void (const ReverbDecisionEngine::Recommendation&)> onRecommendationReady;

    // Thread-safe polling of last result if callback not used
    bool hasNewResult() const noexcept { return resultAvailable.load (std::memory_order_acquire); }
    ReverbDecisionEngine::Recommendation popLatestResult() noexcept;

private:
    void run() override;

    LocalAIModel model;
    juce::WaitableEvent wakeEvent;

    // Lock-free double-buffered job slot for audio thread -> worker
    std::atomic<bool> hasPendingJob { false };
    ProVerbAI::FeatureSnapshot pendingSnapshot;

    // Lock-free result slot for worker -> processor
    std::atomic<bool> resultAvailable { false };
    ReverbDecisionEngine::Recommendation latestRecommendation;
    juce::SpinLock resultLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AIInferenceWorker)
};
