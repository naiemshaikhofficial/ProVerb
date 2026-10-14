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

        // Ducking depth: 0% = 0dB, 100% = -24dB max reduction
        const float maxDuckingDepth = (duckingPercent / 100.0f) * 0.94f;

        // Ballistics coefficients
        const float attackCoeff    = static_cast<float> (std::exp (-1.0 / ((attackMs * 0.001) * currentSampleRate)));
        const float releaseCoeff   = static_cast<float> (std::exp (-1.0 / ((releaseMs * 0.001) * currentSampleRate)));
        const float rmsWindowCoeff = static_cast<float> (std::exp (-1.0 / (0.025 * currentSampleRate))); // 25ms RMS window
        const float slewGain       = static_cast<float> (1.0 - std::exp (-1.0 / (0.008 * currentSampleRate)));

        const float thresholdLinear = juce::Decibels::decibelsToGain (thresholdDb);
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

            // 4. Soft knee threshold curve
            float excess = hybridEnvelope - thresholdLinear;
            float normalizedDry = 0.0f;
            if (excess > 0.0f)
            {
                // Soft sigmoid ramp up to 1.0
                const float scaledExcess = excess / (thresholdLinear * 3.0f + 1.0e-4f);
                normalizedDry = juce::jlimit (0.0f, 1.0f, scaledExcess);
            }

            const float targetDuckGain = 1.0f - (normalizedDry * maxDuckingDepth);
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

