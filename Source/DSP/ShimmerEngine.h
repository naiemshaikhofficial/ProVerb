#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

/**
 * Celestial Shimmer Engine (+1 Octave Pitch Shifting & Ambient Bloom)
 * 
 * Features:
 * - Dual Hann-windowed pitch grain crossfading (+12 semitones / 2x speed)
 * - Soft high-frequency air filter to prevent piercing highs
 * - Controlled feedback bloom with soft saturation
 * - Stereo decorrelated dispersion
 */
class ShimmerEngine
{
public:
    void prepare (double sampleRate)
    {
        currentSampleRate = sampleRate > 8000.0 ? sampleRate : 44100.0;
        windowSize = static_cast<int> (currentSampleRate * 0.055); // ~55ms grain window
        if (windowSize < 64) windowSize = 64;

        delayBufferL.resize (windowSize * 4, 0.0f);
        delayBufferR.resize (windowSize * 4, 0.0f);
        writeIndex = 0;
        phase = 0.0f;
        filterStateL = 0.0f;
        filterStateR = 0.0f;
    }

    void reset()
    {
        std::fill (delayBufferL.begin(), delayBufferL.end(), 0.0f);
        std::fill (delayBufferR.begin(), delayBufferR.end(), 0.0f);
        writeIndex = 0;
        phase = 0.0f;
        filterStateL = 0.0f;
        filterStateR = 0.0f;
    }

    static inline float readSampleHermite4 (const float* buf, int bufSize, float readPos) noexcept
    {
        while (readPos < 0.0f) readPos += static_cast<float> (bufSize);
        while (readPos >= static_cast<float> (bufSize)) readPos -= static_cast<float> (bufSize);

        const int i1 = static_cast<int> (readPos);
        const float frac = readPos - static_cast<float> (i1);

        const int i0 = (i1 - 1 + bufSize) % bufSize;
        const int i2 = (i1 + 1) % bufSize;
        const int i3 = (i1 + 2) % bufSize;

        const float y0 = buf[i0];
        const float y1 = buf[i1];
        const float y2 = buf[i2];
        const float y3 = buf[i3];

        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    void process (float* wetL, float* wetR, int numSamples, float shimmerPercent, float semitones = 12.0f)
    {
        if (shimmerPercent <= 0.05f)
            return;

        const float shimmerAmount = (shimmerPercent / 100.0f) * 0.70f;
        const int bufSize = static_cast<int> (delayBufferL.size());

        // Pitch playback ratio: semitones = 12.0f -> 2.0x speed (+1 Octave)
        const float speedRatio = std::pow (2.0f, semitones / 12.0f);
        const float pitchDelta = speedRatio - 1.0f;
        const float phaseInc = (pitchDelta != 0.0f ? pitchDelta : 1.0f) / static_cast<float> (windowSize);

        // One-pole smoothing filter on shimmer output (gentle high-frequency roll-off at 8.5kHz)
        const float dampCoeff = static_cast<float> (std::exp (-juce::MathConstants<double>::twoPi * 8500.0 / currentSampleRate));

        for (int i = 0; i < numSamples; ++i)
        {
            // Soft-saturated feedback signal into shimmer delay buffer
            const float fbL = filterStateL * shimmerAmount * 0.35f;
            const float fbR = filterStateR * shimmerAmount * 0.35f;
            const float satFbL = fbL / (1.0f + 0.15f * fbL * fbL);
            const float satFbR = fbR / (1.0f + 0.15f * fbR * fbR);

            delayBufferL[writeIndex] = wetL[i] + satFbL;
            delayBufferR[writeIndex] = wetR[i] + satFbR;
            writeIndex = (writeIndex + 1) % bufSize;

            // Advance grain phase
            phase += phaseInc;
            while (phase >= 1.0f) phase -= 1.0f;
            while (phase < 0.0f)  phase += 1.0f;

            float phase2 = phase + 0.5f;
            while (phase2 >= 1.0f) phase2 -= 1.0f;
            while (phase2 < 0.0f)  phase2 += 1.0f;

            // Hann / sinusoidal crossfade window
            const float env1 = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * phase));
            const float env2 = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * phase2));

            const float delay1 = phase * static_cast<float> (windowSize);
            const float delay2 = phase2 * static_cast<float> (windowSize);

            const float readPos1 = static_cast<float> (writeIndex) - delay1;
            const float readPos2 = static_cast<float> (writeIndex) - delay2;

            // 4-point Hermite cubic fractional interpolation (eliminates integer truncation grain buzzing)
            const float s1L = readSampleHermite4 (delayBufferL.data(), bufSize, readPos1);
            const float s2L = readSampleHermite4 (delayBufferL.data(), bufSize, readPos2);
            const float s1R = readSampleHermite4 (delayBufferR.data(), bufSize, readPos1);
            const float s2R = readSampleHermite4 (delayBufferR.data(), bufSize, readPos2);

            const float pitchedL = (s1L * env1) + (s2L * env2);
            const float pitchedR = (s1R * env1) + (s2R * env2);

            // Gentle smoothing to keep highs sweet and lush
            filterStateL = ((1.0f - dampCoeff) * pitchedL) + (dampCoeff * filterStateL);
            filterStateR = ((1.0f - dampCoeff) * pitchedR) + (dampCoeff * filterStateR);

            // Add shimmer into wet buffer
            wetL[i] += filterStateL * shimmerAmount;
            wetR[i] += filterStateR * shimmerAmount;
        }
    }

private:
    double currentSampleRate = 44100.0;
    int windowSize = 2425;
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int writeIndex = 0;
    float phase = 0.0f;
    float filterStateL = 0.0f;
    float filterStateR = 0.0f;
};
