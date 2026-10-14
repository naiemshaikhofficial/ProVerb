#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "SourceDetectorWeights.h"
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstring>

/**
 * SourceDetector
 * 
 * Multi-Label Real-Time Audio Source Family Detector for ProducerToy Reverb.
 * 
 * Architectural Contract:
 * 1. Audio thread (processBlock) is strictly lock-free and zero-allocation.
 *    Audio is pushed via SPSC juce::AbstractFifo ring buffer.
 * 2. Background worker thread runs rate-bounded (~25 Hz) at low priority.
 * 3. Exact 1.000s rolling window (44,100 samples) resampled to 44.1 kHz.
 * 4. 150 ms hop interval (6,615 samples @ 44.1 kHz).
 * 5. Multi-label inference over 8 broad source families + Mixed/Unknown fallbacks.
 * 6. Temporal probability smoothing, hysteresis, and hold-time state machine.
 */
class SourceDetector : public juce::Thread
{
public:
    static constexpr int ANALYSIS_WINDOW_SAMPLES = 44100; // 1.000s window
    static constexpr int HOP_SAMPLES             = 6615;  // 150 ms hop interval
    static constexpr int FIFO_CAPACITY           = 65536; // SPSC ring buffer (~1.5s @ 44.1k)
    static constexpr int NUM_FAMILIES            = 8;
    static constexpr int FAMILY_MIXED            = 8;
    static constexpr int FAMILY_UNKNOWN          = 9;

    SourceDetector()
        : juce::Thread ("ProducerToy_SourceDetectorWorker"),
          fifo (FIFO_CAPACITY)
    {
        fifoBuffer.resize (static_cast<size_t> (FIFO_CAPACITY), 0.0f);
        consumerBuffer.resize (4096, 0.0f);
        rollingWindow.resize (static_cast<size_t> (ANALYSIS_WINDOW_SAMPLES), 0.0f);
        resampleHistoryBuffer.reserve (4096);
        resampleHistoryBuffer.assign (1, 0.0f);
        resamplePhase = 1.0;
        resampledChunk.reserve (8192);

        for (int i = 0; i < NUM_FAMILIES; ++i)
        {
            smoothedProbs[i].store (0.0f, std::memory_order_relaxed);
            rawProbs[i].store (0.0f, std::memory_order_relaxed);
        }
        dominantFamily.store (FAMILY_UNKNOWN, std::memory_order_relaxed);
        dominantConfidence.store (0.0f, std::memory_order_relaxed);
    }

    ~SourceDetector() override
    {
        stop();
    }

    void prepare (double sampleRate)
    {
        stop();

        fifo.reset();
        std::fill (fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
        std::fill (rollingWindow.begin(), rollingWindow.end(), 0.0f);
        resampleHistoryBuffer.assign (1, 0.0f);
        resamplePhase = 1.0;
        samplesPrimed = 0;
        samplesSinceLastHop = 0;
        currentSampleRate = sampleRate > 8000.0 ? sampleRate : 44100.0;

        for (int i = 0; i < NUM_FAMILIES; ++i)
        {
            smoothedProbs[i].store (0.0f, std::memory_order_relaxed);
            rawProbs[i].store (0.0f, std::memory_order_relaxed);
        }
        dominantFamily.store (FAMILY_UNKNOWN, std::memory_order_relaxed);
        dominantConfidence.store (0.0f, std::memory_order_relaxed);
        pendingFamily = FAMILY_UNKNOWN;
        pendingHoldHops = 0;

        startThread (juce::Thread::Priority::low);
    }

    void stop()
    {
        if (isThreadRunning())
        {
            signalThreadShouldExit();
            notify();
            stopThread (1000);
        }
    }

    void reset()
    {
        fifo.reset();
        std::fill (fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
        std::fill (rollingWindow.begin(), rollingWindow.end(), 0.0f);
        resampleHistoryBuffer.assign (1, 0.0f);
        resamplePhase = 1.0;
        samplesPrimed = 0;
        samplesSinceLastHop = 0;

        for (int i = 0; i < NUM_FAMILIES; ++i)
        {
            smoothedProbs[i].store (0.0f, std::memory_order_relaxed);
            rawProbs[i].store (0.0f, std::memory_order_relaxed);
        }
        dominantFamily.store (FAMILY_UNKNOWN, std::memory_order_relaxed);
        dominantConfidence.store (0.0f, std::memory_order_relaxed);
        pendingFamily = FAMILY_UNKNOWN;
        pendingHoldHops = 0;
    }

    void setEnabled (bool shouldBeEnabled) noexcept
    {
        enabled.store (shouldBeEnabled, std::memory_order_relaxed);
    }

    bool isEnabled() const noexcept
    {
        return enabled.load (std::memory_order_relaxed);
    }

    // Lock-free zero-allocation write on real-time audio thread
    void pushAudio (const juce::AudioBuffer<float>& buffer, int actualInputChannels = -1) noexcept
    {
        if (! enabled.load (std::memory_order_relaxed))
            return;

        const int bufferChannels = buffer.getNumChannels();
        const int numChannels = (actualInputChannels > 0)
                                    ? std::min (actualInputChannels, bufferChannels)
                                    : bufferChannels;
        const int numSamples  = buffer.getNumSamples();
        if (numChannels <= 0 || numSamples <= 0)
            return;

        float monoChunk[512];
        int remaining = numSamples;
        int srcOffset = 0;

        while (remaining > 0)
        {
            const int chunkSize = std::min (remaining, 512);

            if (numChannels == 1)
            {
                const float* inL = buffer.getReadPointer (0, srcOffset);
                std::memcpy (monoChunk, inL, static_cast<size_t> (chunkSize) * sizeof (float));
            }
            else
            {
                const float* inL = buffer.getReadPointer (0, srcOffset);
                const float* inR = buffer.getReadPointer (1, srcOffset);
                for (int i = 0; i < chunkSize; ++i)
                {
                    monoChunk[i] = (inL[i] + inR[i]) * 0.5f;
                }
            }

            int start1, size1, start2, size2;
            fifo.prepareToWrite (chunkSize, start1, size1, start2, size2);

            if (size1 + size2 < chunkSize)
            {
                // SPSC FIFO overflow safeguard
                fifo.finishedWrite (0);
                return;
            }

            if (size1 > 0)
                std::memcpy (fifoBuffer.data() + start1, monoChunk, static_cast<size_t> (size1) * sizeof (float));
            if (size2 > 0)
                std::memcpy (fifoBuffer.data() + start2, monoChunk + size1, static_cast<size_t> (size2) * sizeof (float));

            fifo.finishedWrite (chunkSize);

            srcOffset += chunkSize;
            remaining -= chunkSize;
        }
    }

    // Thread-safe atomic getters for UI and Processor
    int getDominantFamily() const noexcept
    {
        return dominantFamily.load (std::memory_order_relaxed);
    }

    float getDominantConfidence() const noexcept
    {
        return dominantConfidence.load (std::memory_order_relaxed);
    }

    float getFamilyProbability (int familyIndex) const noexcept
    {
        if (familyIndex >= 0 && familyIndex < NUM_FAMILIES)
            return smoothedProbs[familyIndex].load (std::memory_order_relaxed);
        return 0.0f;
    }

    static const char* getFamilyName (int familyIndex) noexcept
    {
        switch (familyIndex)
        {
            case 0: return "Voice";
            case 1: return "Drums & Percussion";
            case 2: return "Bass";
            case 3: return "Keys & Mallets";
            case 4: return "Plucked Strings";
            case 5: return "Bowed Strings";
            case 6: return "Winds & Brass";
            case 7: return "Electronic";
            case FAMILY_MIXED:   return "Mixed / Ensemble";
            case FAMILY_UNKNOWN: return "Unknown / Low Confidence";
            default:             return "Unknown";
        }
    }

    static const char* getFamilyAliases (int familyIndex) noexcept
    {
        switch (familyIndex)
        {
            case 0: return "Lead Vocal, Rap/Spoken, Backing Vocals, Harmonies, Choir";
            case 1: return "Kick, Snare, Toms, Cymbals, Tabla, Dholak, Dhol, Cajon, Congas, Bongos, Frame Drum, Percussion";
            case 2: return "Electric Bass, Upright/Acoustic Bass, Synth Bass, Sub Bass";
            case 3: return "Grand Piano, Upright Piano, Rhodes, Organ, Harmonium, Celesta, Marimba, Vibraphone";
            case 4: return "Acoustic Guitar, Electric Guitar, Sitar, Sarod, Veena, Tanpura, Oud, Saz, Mandolin, Banjo, Harp";
            case 5: return "Solo Violin, Viola, Cello, Double Bass, Pizzicato, String Section, Orchestral Strings";
            case 6: return "Flute, Bansuri, Shakuhachi, Ney, Duduk, Clarinet, Saxophone, Trumpet, Trombone, Horns";
            case 7: return "Synth Lead, Pluck, Arpeggio, Pad, Texture, Sound Design FX";
            case FAMILY_MIXED:   return "Polyphonic Audio, Mixed Instruments, Song Master / Stem Mix";
            case FAMILY_UNKNOWN: return "Low Level / Silence, Non-Musical Audio, Ambience";
            default:             return "";
        }
    }

    static const char* getRecommendedPresetName (int familyIndex) noexcept
    {
        switch (familyIndex)
        {
            case 0: return "Modern Billboard Pop Vocal";
            case 1: return "Snare Drum - Tight Studio Room";
            case 2: return "Bass Dry / Keep the Low End Tight";
            case 3: return "Grand Piano - Concert Hall";
            case 4: return "Steel-String Acoustic Guitar";
            case 5: return "String Section - Lush Scoring Stage";
            case 6: return "Flute & Bansuri - Breath of Sky";
            case 7: return "Synth Lead & Pluck - Polished Modern";
            case FAMILY_MIXED:   return "Small Acoustic Ensemble - Intimate Glue";
            default:             return nullptr; // Retain user preset
        }
    }

    // Exact 12-feature extractor matching Python pipeline
    static void extractFeatures (const float* audio, int numSamples, double sampleRate,
                                 std::array<float, SourceDetectorWeights::NUM_FEATURES>& feats,
                                 bool& isSilence) noexcept
    {
        if (numSamples <= 0 || audio == nullptr)
        {
            isSilence = true;
            std::fill (feats.begin(), feats.end(), 0.0f);
            return;
        }

        double sumSq = 0.0;
        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float a = std::abs (audio[i]);
            if (a > peak) peak = a;
            sumSq += static_cast<double> (audio[i] * audio[i]);
        }

        const float rms = static_cast<float> (std::sqrt (sumSq / static_cast<double> (numSamples)));
        if (rms < 0.002f)
        {
            isSilence = true;
            std::fill (feats.begin(), feats.end(), 0.0f);
            return;
        }
        isSilence = false;

        // 5-Band Filter Bank (1-pole IIR)
        const double twoPi = juce::MathConstants<double>::twoPi;
        const double alpha1 = 1.0 - std::exp (-twoPi * 95.0 / sampleRate);
        const double alpha2 = 1.0 - std::exp (-twoPi * 320.0 / sampleRate);
        const double alpha3 = 1.0 - std::exp (-twoPi * 1800.0 / sampleRate);
        const double alpha4 = 1.0 - std::exp (-twoPi * 6500.0 / sampleRate);

        double lp1 = 0.0, lp2 = 0.0, lp3 = 0.0, lp4 = 0.0;
        double subE = 0.0, bassE = 0.0, coreE = 0.0, presE = 0.0, airE = 0.0;

        const double fastCoeff = 1.0 - std::exp (-1.0 / (0.003 * sampleRate));
        const double slowCoeff = 1.0 - std::exp (-1.0 / (0.045 * sampleRate));
        double envFast = 0.0, envSlow = 0.0;
        double envDiffSum = 0.0;

        const int refractorySamples = static_cast<int> (sampleRate * 0.050);
        int transientHits = 0;
        int lastHit = -refractorySamples;
        double attackSlopeSum = 0.0;
        float maxEnvSlow = 0.0f;

        int crossings = (audio[0] < 0.0f) ? 1 : 0;

        // Pass 1: Filter bank, dynamics, transient detection
        for (int i = 0; i < numSamples; ++i)
        {
            const double x = static_cast<double> (audio[i]);
            const double absX = std::abs (x);

            lp1 += alpha1 * (x - lp1);
            lp2 += alpha2 * (x - lp2);
            lp3 += alpha3 * (x - lp3);
            lp4 += alpha4 * (x - lp4);

            const double bSub  = lp1;
            const double bBass = lp2 - lp1;
            const double bCore = lp3 - lp2;
            const double bPres = lp4 - lp3;
            const double bAir  = x - lp4;

            subE  += bSub * bSub;
            bassE += bBass * bBass;
            coreE += bCore * bCore;
            presE += bPres * bPres;
            airE  += bAir * bAir;

            if (i > 0)
            {
                const float prev = audio[i - 1];
                const float cur  = audio[i];
                if ((prev >= 0.0f && cur < 0.0f) || (prev < 0.0f && cur >= 0.0f))
                    crossings++;
            }

            envFast += fastCoeff * (absX - envFast);
            envSlow += slowCoeff * (absX - envSlow);
            const double diff = envFast - envSlow;
            envDiffSum += diff * diff;

            if (static_cast<float> (envSlow) > maxEnvSlow)
                maxEnvSlow = static_cast<float> (envSlow);

            if ((envFast > (envSlow * 2.2 + 0.015)) && (absX > 0.02))
            {
                if (i - lastHit > refractorySamples)
                {
                    transientHits++;
                    attackSlopeSum += diff;
                    lastHit = i;
                }
            }
        }

        const double totE = subE + bassE + coreE + presE + airE + 1.0e-12;
        const float crest = (rms > 1.0e-5f) ? (peak / rms) : 1.0f;
        const float zcr   = static_cast<float> (crossings) / static_cast<float> (juce::jmax (1, numSamples));
        const float durationSec = static_cast<float> (numSamples) / static_cast<float> (sampleRate);
        const float transDensity = static_cast<float> (transientHits) / juce::jmax (0.5f, durationSec);
        const float envDynamics  = static_cast<float> (envDiffSum * 10000.0) / static_cast<float> (juce::jmax (1, numSamples));

        const float centroid = static_cast<float> (
            (subE * 50.0 + bassE * 200.0 + coreE * 1000.0 + presE * 4000.0 + airE * 10000.0) / (totE * 10000.0));
        const float avgAttackSlope = transientHits > 0 ? static_cast<float> (attackSlopeSum / transientHits) : 0.0f;

        // Pass 2: Sustain ratio (duty cycle of slow envelope above 35% of max)
        int sustainedSamples = 0;
        const double sustainThresh = maxEnvSlow * 0.35;
        envSlow = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const double absX = std::abs (static_cast<double> (audio[i]));
            envSlow += slowCoeff * (absX - envSlow);
            if (envSlow > sustainThresh)
                sustainedSamples++;
        }
        const float sustainRatio = static_cast<float> (sustainedSamples) / static_cast<float> (juce::jmax (1, numSamples));

        feats[0]  = juce::jlimit (0.0f, 1.0f, static_cast<float> (subE / totE));
        feats[1]  = juce::jlimit (0.0f, 1.0f, static_cast<float> (bassE / totE));
        feats[2]  = juce::jlimit (0.0f, 1.0f, static_cast<float> (coreE / totE));
        feats[3]  = juce::jlimit (0.0f, 1.0f, static_cast<float> (presE / totE));
        feats[4]  = juce::jlimit (0.0f, 1.0f, static_cast<float> (airE / totE));
        feats[5]  = juce::jlimit (1.0f, 50.0f, crest);
        feats[6]  = juce::jlimit (0.0f, 1.0f, zcr);
        feats[7]  = juce::jlimit (0.0f, 30.0f, transDensity);
        feats[8]  = juce::jlimit (0.0f, 100.0f, envDynamics);
        feats[9]  = juce::jlimit (0.0f, 1.0f, centroid);
        feats[10] = juce::jlimit (0.0f, 10.0f, avgAttackSlope * 10.0f);
        feats[11] = juce::jlimit (0.0f, 1.0f, sustainRatio);
    }

    // Multi-label forward pass (12 -> 32 -> 16 -> 8 Sigmoid)
    static void forwardPass (const std::array<float, SourceDetectorWeights::NUM_FEATURES>& rawFeats,
                             std::array<float, NUM_FAMILIES>& outProbs) noexcept
    {
        float xNorm[SourceDetectorWeights::NUM_FEATURES];
        for (int i = 0; i < SourceDetectorWeights::NUM_FEATURES; ++i)
        {
            xNorm[i] = (rawFeats[i] - SourceDetectorWeights::FEATURE_MEANS[i]) / SourceDetectorWeights::FEATURE_STDS[i];
        }

        // Layer 1 (12 -> 32 + ReLU)
        float h1[SourceDetectorWeights::LAYER1_SIZE];
        for (int i = 0; i < SourceDetectorWeights::LAYER1_SIZE; ++i)
        {
            float sum = SourceDetectorWeights::B1[i];
            for (int j = 0; j < SourceDetectorWeights::NUM_FEATURES; ++j)
                sum += SourceDetectorWeights::W1[i][j] * xNorm[j];
            h1[i] = sum > 0.0f ? sum : 0.0f;
        }

        // Layer 2 (32 -> 16 + ReLU)
        float h2[SourceDetectorWeights::LAYER2_SIZE];
        for (int i = 0; i < SourceDetectorWeights::LAYER2_SIZE; ++i)
        {
            float sum = SourceDetectorWeights::B2[i];
            for (int j = 0; j < SourceDetectorWeights::LAYER1_SIZE; ++j)
                sum += SourceDetectorWeights::W2[i][j] * h1[j];
            h2[i] = sum > 0.0f ? sum : 0.0f;
        }

        // Layer 3 (16 -> 8 + Sigmoid)
        for (int i = 0; i < NUM_FAMILIES; ++i)
        {
            float sum = SourceDetectorWeights::B3[i];
            for (int j = 0; j < SourceDetectorWeights::LAYER2_SIZE; ++j)
                sum += SourceDetectorWeights::W3[i][j] * h2[j];
            outProbs[i] = 1.0f / (1.0f + std::exp (-sum));
        }
    }

private:
    void run() override
    {
        while (! threadShouldExit())
        {
            wait (40);
            if (threadShouldExit())
                break;

            if (! enabled.load (std::memory_order_relaxed))
                continue;

            const int numAvailable = fifo.getNumReady();
            if (numAvailable <= 0)
                continue;

            int start1, size1, start2, size2;
            fifo.prepareToRead (numAvailable, start1, size1, start2, size2);

            const int totalRead = size1 + size2;
            if (totalRead > static_cast<int> (consumerBuffer.size()))
                consumerBuffer.resize (static_cast<size_t> (totalRead));

            if (size1 > 0)
                std::memcpy (consumerBuffer.data(), fifoBuffer.data() + start1, static_cast<size_t> (size1) * sizeof (float));
            if (size2 > 0)
                std::memcpy (consumerBuffer.data() + size1, fifoBuffer.data() + start2, static_cast<size_t> (size2) * sizeof (float));

            fifo.finishedRead (totalRead);

            // Resample to 44.1 kHz if necessary
            resampledChunk.clear();
            const bool needsResample = std::abs (currentSampleRate - 44100.0) >= 1.0;
            if (! needsResample)
            {
                resampledChunk.insert (resampledChunk.end(), consumerBuffer.begin(), consumerBuffer.begin() + totalRead);
            }
            else
            {
                // Cubic Hermite resampler
                const double inSR = currentSampleRate;
                const double outSR = 44100.0;
                const double ratio = inSR / outSR;
                int inIdx = 0;

                while (inIdx < totalRead)
                {
                    while (resamplePhase < 1.0 && inIdx < totalRead)
                    {
                        resampleHistoryBuffer.push_back (consumerBuffer[static_cast<size_t> (inIdx)]);
                        if (resampleHistoryBuffer.size() > 4)
                            resampleHistoryBuffer.erase (resampleHistoryBuffer.begin());
                        resamplePhase += 1.0;
                        inIdx++;
                    }

                    while (resamplePhase >= 1.0)
                    {
                        const size_t histSize = resampleHistoryBuffer.size();
                        const float p0 = histSize >= 4 ? resampleHistoryBuffer[histSize - 4] : 0.0f;
                        const float p1 = histSize >= 3 ? resampleHistoryBuffer[histSize - 3] : 0.0f;
                        const float p2 = histSize >= 2 ? resampleHistoryBuffer[histSize - 2] : 0.0f;
                        const float p3 = histSize >= 1 ? resampleHistoryBuffer[histSize - 1] : 0.0f;

                        const float t = static_cast<float> (resamplePhase - 1.0);
                        const float a0 = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
                        const float a1 =  p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
                        const float a2 = -0.5f * p0 + 0.5f * p2;
                        const float a3 =  p1;

                        resampledChunk.push_back (((a0 * t + a1) * t + a2) * t + a3);
                        resamplePhase -= ratio;
                    }
                }
            }

            // Stream into rolling window
            const int numResampled = static_cast<int> (resampledChunk.size());
            int chunkOffset = 0;

            while (chunkOffset < numResampled)
            {
                if (samplesPrimed < ANALYSIS_WINDOW_SAMPLES)
                {
                    const int neededToPrime = ANALYSIS_WINDOW_SAMPLES - samplesPrimed;
                    const int toTake = std::min (neededToPrime, numResampled - chunkOffset);

                    std::memcpy (rollingWindow.data() + samplesPrimed,
                                 resampledChunk.data() + chunkOffset,
                                 static_cast<size_t> (toTake) * sizeof (float));

                    samplesPrimed += toTake;
                    chunkOffset += toTake;

                    if (samplesPrimed == ANALYSIS_WINDOW_SAMPLES)
                    {
                        samplesSinceLastHop = 0;
                        evaluateInference();
                    }
                    continue;
                }

                const int samplesToNextHop = HOP_SAMPLES - samplesSinceLastHop;
                const int toAdvance = std::min (samplesToNextHop, numResampled - chunkOffset);

                if (toAdvance > 0)
                {
                    const int keep = ANALYSIS_WINDOW_SAMPLES - toAdvance;
                    std::memmove (rollingWindow.data(), rollingWindow.data() + toAdvance, static_cast<size_t> (keep) * sizeof (float));
                    std::memcpy (rollingWindow.data() + keep, resampledChunk.data() + chunkOffset, static_cast<size_t> (toAdvance) * sizeof (float));

                    samplesSinceLastHop += toAdvance;
                    chunkOffset += toAdvance;
                }

                if (samplesSinceLastHop >= HOP_SAMPLES)
                {
                    samplesSinceLastHop -= HOP_SAMPLES;
                    evaluateInference();
                }
            }
        }
    }

    void evaluateInference()
    {
        std::array<float, SourceDetectorWeights::NUM_FEATURES> feats {};
        bool isSilence = false;
        extractFeatures (rollingWindow.data(), ANALYSIS_WINDOW_SAMPLES, 44100.0, feats, isSilence);

        std::array<float, NUM_FAMILIES> probs {};
        if (! isSilence)
            forwardPass (feats, probs);
        else
            std::fill (probs.begin(), probs.end(), 0.0f);

        int activeCount = 0;
        int topFamily = -1;
        float maxProb = 0.0f;
        int secondFamily = -1;
        float secondProb = 0.0f;

        for (int i = 0; i < NUM_FAMILIES; ++i)
        {
            rawProbs[i].store (probs[i], std::memory_order_relaxed);
            const float prev = smoothedProbs[i].load (std::memory_order_relaxed);
            const float updated = prev * 0.80f + probs[i] * 0.20f;
            smoothedProbs[i].store (updated, std::memory_order_relaxed);

            if (updated >= SourceDetectorWeights::THRESHOLDS[i])
            {
                activeCount++;
            }

            if (updated > maxProb)
            {
                secondProb = maxProb;
                secondFamily = topFamily;
                maxProb = updated;
                topFamily = i;
            }
            else if (updated > secondProb)
            {
                secondProb = updated;
                secondFamily = i;
            }
        }

        // Multi-Label Arbitration:
        // Distinguish single dominant instrument vs. genuine multi-instrument polyphonic mix
        int candidateFamily = FAMILY_UNKNOWN;
        float candidateConf = 0.0f;

        if (isSilence)
        {
            candidateFamily = FAMILY_UNKNOWN;
            candidateConf = 0.0f;
        }
        else if (activeCount >= 2 && (maxProb - secondProb) < 0.09f && secondProb >= 0.26f)
        {
            // Two or more active families with competing close probabilities -> Mixed / Ensemble
            candidateFamily = FAMILY_MIXED;
            candidateConf = (maxProb + secondProb) * 0.5f;
        }
        else if (topFamily >= 0 && maxProb >= SourceDetectorWeights::THRESHOLDS[topFamily])
        {
            // Decisive dominant family exceeding trained threshold
            candidateFamily = topFamily;
            candidateConf = maxProb;
        }
        else if (topFamily >= 0 && maxProb >= 0.24f)
        {
            candidateFamily = topFamily;
            candidateConf = maxProb;
        }
        else
        {
            candidateFamily = FAMILY_UNKNOWN;
            candidateConf = maxProb;
        }

        // Hold-time and hysteresis state machine (responsive 3 hops = ~450ms)
        if (candidateFamily == pendingFamily)
        {
            pendingHoldHops++;
            if (pendingHoldHops >= 3)
            {
                dominantFamily.store (candidateFamily, std::memory_order_release);
                dominantConfidence.store (candidateConf, std::memory_order_release);
            }
        }
        else
        {
            pendingFamily = candidateFamily;
            pendingHoldHops = 1;
            dominantConfidence.store (candidateConf, std::memory_order_release);
        }
    }

    std::atomic<bool> enabled { true };
    double currentSampleRate { 44100.0 };

    // SPSC Ring Buffer
    juce::AbstractFifo fifo;
    std::vector<float> fifoBuffer;
    std::vector<float> consumerBuffer;

    // Rolling Window & Resampler
    std::vector<float> rollingWindow;
    int samplesPrimed { 0 };
    int samplesSinceLastHop { 0 };
    std::vector<float> resampleHistoryBuffer;
    double resamplePhase { 1.0 };
    std::vector<float> resampledChunk;

    // Atomic Outputs
    std::atomic<float> rawProbs[NUM_FAMILIES];
    std::atomic<float> smoothedProbs[NUM_FAMILIES];
    std::atomic<int> dominantFamily { FAMILY_UNKNOWN };
    std::atomic<float> dominantConfidence { 0.0f };

    // Hysteresis & hold state
    int pendingFamily { FAMILY_UNKNOWN };
    int pendingHoldHops { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceDetector)
};
