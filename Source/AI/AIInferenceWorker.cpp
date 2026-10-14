#include "AIInferenceWorker.h"

AIInferenceWorker::AIInferenceWorker()
    : juce::Thread ("ProVerb_AIWorker")
{
}

AIInferenceWorker::~AIInferenceWorker()
{
    stopWorker();
}

void AIInferenceWorker::startWorker()
{
    if (!isThreadRunning())
        startThread (juce::Thread::Priority::normal);
}

void AIInferenceWorker::stopWorker()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();
        wakeEvent.signal();
        stopThread (3000);
    }
}

bool AIInferenceWorker::loadModel (const juce::File& file)
{
    return model.loadFromFile (file);
}

bool AIInferenceWorker::autoDiscoverModel()
{
    return model.autoDiscoverAndLoad();
}

juce::String AIInferenceWorker::getModelStatusText() const
{
    if (model.isLoaded())
        return "Local AI Ready (" + juce::File (model.getModelPath()).getFileName() + ")";

    if (model.getStatus() == LocalAIModel::ModelStatus::LoadFailed)
        return "AI Error: " + model.getLastError();

    return "AI Model Not Found (Heuristic Engine Active)";
}

bool AIInferenceWorker::postAnalysisJob (const ProVerbAI::FeatureSnapshot& snapshot) noexcept
{
    // Wait-free audio thread handoff
    pendingSnapshot = snapshot;
    hasPendingJob.store (true, std::memory_order_release);
    wakeEvent.signal();
    return true;
}

ReverbDecisionEngine::Recommendation AIInferenceWorker::popLatestResult() noexcept
{
    const juce::SpinLock::ScopedLockType sl (resultLock);
    resultAvailable.store (false, std::memory_order_release);
    return latestRecommendation;
}

void AIInferenceWorker::run()
{
    while (!threadShouldExit())
    {
        wakeEvent.wait (-1);

        if (threadShouldExit())
            break;

        if (hasPendingJob.exchange (false, std::memory_order_acq_rel))
        {
            auto snapshot = pendingSnapshot;
            snapshot.sanitize();

            ReverbDecisionEngine::Recommendation rec;

            if (snapshot.isSilence || snapshot.meanRms < 0.002)
            {
                // Explicit silence handling: bypass model inference on silence
                std::array<float, ProVerbAI::numClasses> dummyProbs {};
                rec = ReverbDecisionEngine::decideFromModel (dummyProbs, snapshot);
            }
            else if (model.isLoaded())
            {
                std::array<float, ProVerbAI::numClasses> probabilities {};
                if (model.predict (snapshot.features, probabilities))
                {
                    rec = ReverbDecisionEngine::decideFromModel (probabilities, snapshot);
                }
                else
                {
                    // Fallback to heuristic if model inference encounters an error
                    rec = ReverbDecisionEngine::decideFromHeuristics (snapshot);
                }
            }
            else
            {
                // Heuristic engine fallback when no .onnx model is loaded
                rec = ReverbDecisionEngine::decideFromHeuristics (snapshot);
            }

            {
                const juce::SpinLock::ScopedLockType sl (resultLock);
                latestRecommendation = rec;
                resultAvailable.store (true, std::memory_order_release);
            }

            if (onRecommendationReady)
                onRecommendationReady (rec);
        }
    }
}
