#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <atomic>
#include <algorithm>

/**
 * Smart Vocal & Drum Reverb Ducking Engine
 * 
 * Features:
 * - True dual Peak + RMS hybrid sidechain envelope detector
 * - Smooth threshold sensitivity with musical logarithmic knee
 * - 8ms attack, 160ms release ballistics
 * - Exposed real-time gain-reduction metering
 * - Smooth sample-by-sample wet reverb attenuation
 */
class DuckingEngine
{
public:
    void prepare (double sampleRate)
    {
        currentSampleRate = sampleRate > 8000.0 ? sampleRate : 44100.0;
        reset();
    }

    void reset()
    {
        peakEnvelope = 0.0f;
        rmsSquareEnv = 0.0f;
        smoothedGainReduction = 1.0f;
        currentGainReductionDb.store (0.0f, std::memory_order_relaxed);
    }

    // Process with existing public signature
    void process (const float* dryL, const float* dryR, float* wetL, float* wetR, int numSamples,
                  float duckingPercent, float thresholdDb = -32.0f, float attackMs = 8.0f, float releaseMs = 160.0f)
    {
        if (numSamples == 0)
            return;

        if (duckingPercent <= 0.05f)
        {
            smoothedGainReduction = 1.0f;
            currentGainReductionDb.store (0.0f, std::memory_order_relaxed);
            return;
        }

        // Maximum ducking depth: 0% = 0dB, 100% = -28dB max reduction
        const float maxDuckingDb = (duckingPercent / 100.0f) * 28.0f;
        const float duckRatio = juce::jlimit (0.2f, 1.0f, duckingPercent / 100.0f);

        // Ballistics coefficients
        const float attackCoeff    = static_cast<float> (std::exp (-1.0 / (juce::jmax (0.0005, attackMs * 0.001) * currentSampleRate)));
        const float releaseCoeff   = static_cast<float> (std::exp (-1.0 / (juce::jmax (0.010, releaseMs * 0.001) * currentSampleRate)));
        const float rmsWindowCoeff = static_cast<float> (std::exp (-1.0 / (0.025 * currentSampleRate))); // 25ms RMS window
        const float slewGain       = static_cast<float> (1.0 - std::exp (-1.0 / (0.006 * currentSampleRate)));

        constexpr float kneeHalfDb = 3.0f;
        const float lowerBoundDb = thresholdDb - kneeHalfDb;
        const float upperBoundDb = thresholdDb + kneeHalfDb;

        float minGainInBlock = 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = dryL[i];
            const float inR = dryR[i];

            // 1. Peak detector
            const float instPeak = std::max (std::abs (inL), std::abs (inR));
            if (instPeak > peakEnvelope)
                peakEnvelope = attackCoeff * peakEnvelope + (1.0f - attackCoeff) * instPeak;
            else
                peakEnvelope = releaseCoeff * peakEnvelope + (1.0f - releaseCoeff) * instPeak;

            // 2. Mean-square RMS detector
            const float instSquare = 0.5f * (inL * inL + inR * inR);
            rmsSquareEnv = rmsWindowCoeff * rmsSquareEnv + (1.0f - rmsWindowCoeff) * instSquare;
            const float instRms = std::sqrt (std::max (1.0e-9f, rmsSquareEnv));

            // 3. True Peak + RMS Hybrid Envelope (50% Peak for transients + 50% RMS for vocal body)
            const float hybridEnvelope = (0.50f * peakEnvelope) + (0.50f * instRms);

            // 4. Logarithmic Decibel Soft-Knee Compression Transfer Function
            const float envDb = juce::Decibels::gainToDecibels (std::max (1.0e-5f, hybridEnvelope));
            float targetGrDb = 0.0f;

            if (envDb > lowerBoundDb)
            {
                if (envDb < upperBoundDb)
                {
                    // Soft parabolic knee
                    const float delta = envDb - lowerBoundDb;
                    const float excess = (delta * delta) / (4.0f * kneeHalfDb);
                    targetGrDb = -juce::jmin (maxDuckingDb, excess * duckRatio * 0.85f);
                }
                else
                {
                    // Linear dB reduction above knee
                    const float excess = envDb - thresholdDb;
                    targetGrDb = -juce::jmin (maxDuckingDb, excess * duckRatio * 0.85f);
                }
            }

            const float targetDuckGain = juce::Decibels::decibelsToGain (targetGrDb);
            smoothedGainReduction += slewGain * (targetDuckGain - smoothedGainReduction);

            minGainInBlock = std::min (minGainInBlock, smoothedGainReduction);

            wetL[i] *= smoothedGainReduction;
            wetR[i] *= smoothedGainReduction;
        }

        // Thread-safe gain reduction metering in dB (negative value representing cut, e.g. -6.0 dB)
        const float grDb = juce::Decibels::gainToDecibels (std::max (1.0e-4f, minGainInBlock));
        currentGainReductionDb.store (grDb, std::memory_order_relaxed);
    }

    // Live gain reduction readout (thread-safe for UI meters)
    float getGainReductionDb() const noexcept
    {
        return currentGainReductionDb.load (std::memory_order_relaxed);
    }

    float getGainReductionLinear() const noexcept
    {
        return smoothedGainReduction;
    }

private:
    double currentSampleRate = 44100.0;
    float peakEnvelope = 0.0f;
    float rmsSquareEnv = 0.0f;
    float smoothedGainReduction = 1.0f;
    std::atomic<float> currentGainReductionDb { 0.0f };
};

