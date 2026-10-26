#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

/**
 * Modern 5-Band Interactive Reverb EQ Engine
 * 
 * Features:
 * - Band 1: Low Cut (High-Pass, 12 dB/oct smooth State Variable TPT filter)
 * - Band 2: Low Shelf (Parametric shelving biquad with smooth coefficient updates)
 * - Band 3: Mid Bell (Parametric peaking filter with adjustable Frequency, Gain, and Q)
 * - Band 4: High Shelf (Parametric shelving biquad with smooth coefficient updates)
 * - Band 5: High Cut (Low-Pass, 12 dB/oct smooth State Variable TPT filter)
 * - Sub-block processing (32 samples) for click-free de-zippered automation
 * - Unconditionally stable Direct Form II Transposed biquad topology with denormal protection
 * - True stereo dual-channel processing
 */
class ToneEQ
{
public:
    struct Biquad
    {
        float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f };
        float a1 { 0.0f }, a2 { 0.0f };
        float s1 { 0.0f }, s2 { 0.0f };

        void reset() noexcept
        {
            s1 = 0.0f;
            s2 = 0.0f;
        }

        inline float processSample (float in) noexcept
        {
            const float out = b0 * in + s1;
            s1 = b1 * in - a1 * out + s2;
            s2 = b2 * in - a2 * out;
            if (std::abs (s1) < 1.0e-18f) s1 = 0.0f;
            if (std::abs (s2) < 1.0e-18f) s2 = 0.0f;
            return out;
        }

        void setPeaking (double sampleRate, float freq, float gainDb, float Q) noexcept
        {
            if (std::abs (gainDb) < 0.02f)
            {
                b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
                a1 = 0.0f; a2 = 0.0f;
                return;
            }

            const float safeFreq = juce::jlimit (20.0f, static_cast<float> (sampleRate * 0.45), freq);
            const float safeQ = juce::jlimit (0.15f, 10.0f, Q);
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float w0 = static_cast<float> (juce::MathConstants<double>::twoPi * safeFreq / sampleRate);
            const float cosW0 = std::cos (w0);
            const float sinW0 = std::sin (w0);
            const float alpha = sinW0 / (2.0f * safeQ);

            const float a0 = 1.0f + (alpha / A);
            const float invA0 = 1.0f / a0;

            b0 = (1.0f + alpha * A) * invA0;
            b1 = (-2.0f * cosW0) * invA0;
            b2 = (1.0f - alpha * A) * invA0;
            a1 = (-2.0f * cosW0) * invA0;
            a2 = (1.0f - alpha / A) * invA0;
        }

        void setLowShelf (double sampleRate, float freq, float gainDb) noexcept
        {
            if (std::abs (gainDb) < 0.02f)
            {
                b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
                a1 = 0.0f; a2 = 0.0f;
                return;
            }

            const float safeFreq = juce::jlimit (20.0f, static_cast<float> (sampleRate * 0.45), freq);
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float w0 = static_cast<float> (juce::MathConstants<double>::twoPi * safeFreq / sampleRate);
            const float cosW0 = std::cos (w0);
            const float sinW0 = std::sin (w0);
            const float alpha = sinW0 * 0.70710678f;
            const float twoSqrtAAlpha = 2.0f * std::sqrt (A) * alpha;

            const float a0 = (A + 1.0f) + (A - 1.0f) * cosW0 + twoSqrtAAlpha;
            const float invA0 = 1.0f / a0;

            b0 = (A * ((A + 1.0f) - (A - 1.0f) * cosW0 + twoSqrtAAlpha)) * invA0;
            b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0)) * invA0;
            b2 = (A * ((A + 1.0f) - (A - 1.0f) * cosW0 - twoSqrtAAlpha)) * invA0;
            a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0)) * invA0;
            a2 = ((A + 1.0f) + (A - 1.0f) * cosW0 - twoSqrtAAlpha) * invA0;
        }

        void setHighShelf (double sampleRate, float freq, float gainDb) noexcept
        {
            if (std::abs (gainDb) < 0.02f)
            {
                b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
                a1 = 0.0f; a2 = 0.0f;
                return;
            }

            const float safeFreq = juce::jlimit (20.0f, static_cast<float> (sampleRate * 0.45), freq);
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float w0 = static_cast<float> (juce::MathConstants<double>::twoPi * safeFreq / sampleRate);
            const float cosW0 = std::cos (w0);
            const float sinW0 = std::sin (w0);
            const float alpha = sinW0 * 0.70710678f;
            const float twoSqrtAAlpha = 2.0f * std::sqrt (A) * alpha;

            const float a0 = (A + 1.0f) - (A - 1.0f) * cosW0 + twoSqrtAAlpha;
            const float invA0 = 1.0f / a0;

            b0 = (A * ((A + 1.0f) + (A - 1.0f) * cosW0 + twoSqrtAAlpha)) * invA0;
            b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW0)) * invA0;
            b2 = (A * ((A + 1.0f) + (A - 1.0f) * cosW0 - twoSqrtAAlpha)) * invA0;
            a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosW0)) * invA0;
            a2 = ((A + 1.0f) - (A - 1.0f) * cosW0 - twoSqrtAAlpha) * invA0;
        }
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        currentSampleRate = spec.sampleRate > 8000.0 ? spec.sampleRate : 44100.0;

        juce::dsp::ProcessSpec monoSpec;
        monoSpec.sampleRate = currentSampleRate;
        monoSpec.maximumBlockSize = spec.maximumBlockSize;
        monoSpec.numChannels = 1;

        lowCutL.prepare (monoSpec);
        lowCutL.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        lowCutR.prepare (monoSpec);
        lowCutR.setType (juce::dsp::StateVariableTPTFilterType::highpass);

        highCutL.prepare (monoSpec);
        highCutL.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        highCutR.prepare (monoSpec);
        highCutR.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

        lowShelfL.reset();
        lowShelfR.reset();
        midBellL.reset();
        midBellR.reset();
        highShelfL.reset();
        highShelfR.reset();

        // Parameter smoothers (35ms glide)
        smoothLowCutPitch.reset (currentSampleRate, 0.035);
        smoothHighCutPitch.reset (currentSampleRate, 0.035);
        smoothLowCutPitch.setCurrentAndTargetValue (std::log2 (80.0f));
        smoothHighCutPitch.setCurrentAndTargetValue (std::log2 (14000.0f));

        smoothLowShelfFreq.reset (currentSampleRate, 0.035);
        smoothLowShelfGain.reset (currentSampleRate, 0.035);
        smoothLowShelfFreq.setCurrentAndTargetValue (250.0f);
        smoothLowShelfGain.setCurrentAndTargetValue (0.0f);

        smoothMidFreq.reset (currentSampleRate, 0.035);
        smoothMidGain.reset (currentSampleRate, 0.035);
        smoothMidQ.reset (currentSampleRate, 0.035);
        smoothMidFreq.setCurrentAndTargetValue (1200.0f);
        smoothMidGain.setCurrentAndTargetValue (0.0f);
        smoothMidQ.setCurrentAndTargetValue (0.70f);

        smoothHighShelfFreq.reset (currentSampleRate, 0.035);
        smoothHighShelfGain.reset (currentSampleRate, 0.035);
        smoothHighShelfFreq.setCurrentAndTargetValue (5000.0f);
        smoothHighShelfGain.setCurrentAndTargetValue (0.0f);
    }

    void reset()
    {
        lowCutL.reset();
        lowCutR.reset();
        highCutL.reset();
        highCutR.reset();
        lowShelfL.reset();
        lowShelfR.reset();
        midBellL.reset();
        midBellR.reset();
        highShelfL.reset();
        highShelfR.reset();
    }

    void process (juce::AudioBuffer<float>& buffer, float lowCutHz, float highCutHz)
    {
        process (buffer, buffer.getNumSamples(), lowCutHz, highCutHz, 250.0f, 0.0f, 1200.0f, 0.0f, 0.70f, 5000.0f, 0.0f);
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamplesToProcess, float lowCutHz, float highCutHz)
    {
        process (buffer, numSamplesToProcess, lowCutHz, highCutHz, 250.0f, 0.0f, 1200.0f, 0.0f, 0.70f, 5000.0f, 0.0f);
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamplesToProcess,
                  float lowCutHz, float highCutHz,
                  float lowShelfFreq, float lowShelfGainDb,
                  float midFreq, float midGainDb, float midQ,
                  float highShelfFreq, float highShelfGainDb)
    {
        const float nyquistSafetyHz = static_cast<float> (currentSampleRate * 0.45);
        const float safeLowCutHz   = juce::jlimit (20.0f, std::min (1000.0f, nyquistSafetyHz), lowCutHz);
        const float safeHighCutHz  = juce::jlimit (1000.0f, std::min (20000.0f, nyquistSafetyHz), highCutHz);
        const float safeLowShFreq  = juce::jlimit (40.0f, std::min (800.0f, nyquistSafetyHz), lowShelfFreq);
        const float safeLowShGain  = juce::jlimit (-12.0f, 12.0f, lowShelfGainDb);
        const float safeMidFreq    = juce::jlimit (100.0f, std::min (8000.0f, nyquistSafetyHz), midFreq);
        const float safeMidGain    = juce::jlimit (-12.0f, 12.0f, midGainDb);
        const float safeMidQ       = juce::jlimit (0.2f, 8.0f, midQ);
        const float safeHighShFreq = juce::jlimit (1500.0f, std::min (16000.0f, nyquistSafetyHz), highShelfFreq);
        const float safeHighShGain = juce::jlimit (-12.0f, 12.0f, highShelfGainDb);

        smoothLowCutPitch.setTargetValue (std::log2 (safeLowCutHz));
        smoothHighCutPitch.setTargetValue (std::log2 (safeHighCutHz));
        smoothLowShelfFreq.setTargetValue (safeLowShFreq);
        smoothLowShelfGain.setTargetValue (safeLowShGain);
        smoothMidFreq.setTargetValue (safeMidFreq);
        smoothMidGain.setTargetValue (safeMidGain);
        smoothMidQ.setTargetValue (safeMidQ);
        smoothHighShelfFreq.setTargetValue (safeHighShFreq);
        smoothHighShelfGain.setTargetValue (safeHighShGain);

        const int numSamples = std::min (numSamplesToProcess, buffer.getNumSamples());
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0)
            return;

        juce::dsp::AudioBlock<float> fullBlock (buffer.getArrayOfWritePointers(), static_cast<size_t> (numChannels), 0, static_cast<size_t> (numSamples));

        constexpr int subBlockSize = 32;
        for (int offset = 0; offset < numSamples; offset += subBlockSize)
        {
            const int chunkSize = std::min (subBlockSize, numSamples - offset);

            const float curLowCut      = std::exp2 (smoothLowCutPitch.skip (chunkSize));
            const float curHighCut     = std::exp2 (smoothHighCutPitch.skip (chunkSize));
            const float curLowShFreq   = smoothLowShelfFreq.skip (chunkSize);
            const float curLowShGain   = smoothLowShelfGain.skip (chunkSize);
            const float curMidFreq     = smoothMidFreq.skip (chunkSize);
            const float curMidGain     = smoothMidGain.skip (chunkSize);
            const float curMidQ        = smoothMidQ.skip (chunkSize);
            const float curHighShFreq  = smoothHighShelfFreq.skip (chunkSize);
            const float curHighShGain  = smoothHighShelfGain.skip (chunkSize);

            // Update TPT cut filters
            lowCutL.setCutoffFrequency (curLowCut);
            highCutL.setCutoffFrequency (curHighCut);

            // Update Biquad shelving and peak filters
            lowShelfL.setLowShelf (currentSampleRate, curLowShFreq, curLowShGain);
            lowShelfR.b0 = lowShelfL.b0; lowShelfR.b1 = lowShelfL.b1; lowShelfR.b2 = lowShelfL.b2;
            lowShelfR.a1 = lowShelfL.a1; lowShelfR.a2 = lowShelfL.a2;

            midBellL.setPeaking (currentSampleRate, curMidFreq, curMidGain, curMidQ);
            midBellR.b0 = midBellL.b0; midBellR.b1 = midBellL.b1; midBellR.b2 = midBellL.b2;
            midBellR.a1 = midBellL.a1; midBellR.a2 = midBellL.a2;

            highShelfL.setHighShelf (currentSampleRate, curHighShFreq, curHighShGain);
            highShelfR.b0 = highShelfL.b0; highShelfR.b1 = highShelfL.b1; highShelfR.b2 = highShelfL.b2;
            highShelfR.a1 = highShelfL.a1; highShelfR.a2 = highShelfL.a2;

            // 1. Process Low-Cut & High-Cut TPT filters
            auto subBlock = fullBlock.getSubBlock (static_cast<size_t> (offset), static_cast<size_t> (chunkSize));
            auto subBlockL = subBlock.getSingleChannelBlock (0);
            juce::dsp::ProcessContextReplacing<float> contextL (subBlockL);
            lowCutL.process (contextL);
            highCutL.process (contextL);

            if (numChannels > 1)
            {
                lowCutR.setCutoffFrequency (curLowCut);
                highCutR.setCutoffFrequency (curHighCut);

                auto subBlockR = subBlock.getSingleChannelBlock (1);
                juce::dsp::ProcessContextReplacing<float> contextR (subBlockR);
                lowCutR.process (contextR);
                highCutR.process (contextR);
            }

            // 2. Process Shelving and Parametric Mid Bell filters sample-by-sample
            float* dataL = buffer.getWritePointer (0, offset);
            float* dataR = numChannels > 1 ? buffer.getWritePointer (1, offset) : nullptr;

            for (int s = 0; s < chunkSize; ++s)
            {
                float xL = dataL[s];
                xL = lowShelfL.processSample (xL);
                xL = midBellL.processSample (xL);
                xL = highShelfL.processSample (xL);
                dataL[s] = xL;

                if (dataR != nullptr)
                {
                    float xR = dataR[s];
                    xR = lowShelfR.processSample (xR);
                    xR = midBellR.processSample (xR);
                    xR = highShelfR.processSample (xR);
                    dataR[s] = xR;
                }
            }
        }
    }

private:
    double currentSampleRate = 44100.0;
    using Filter = juce::dsp::StateVariableTPTFilter<float>;
    Filter lowCutL;
    Filter lowCutR;
    Filter highCutL;
    Filter highCutR;

    Biquad lowShelfL;
    Biquad lowShelfR;
    Biquad midBellL;
    Biquad midBellR;
    Biquad highShelfL;
    Biquad highShelfR;

    juce::SmoothedValue<float> smoothLowCutPitch;
    juce::SmoothedValue<float> smoothHighCutPitch;
    juce::SmoothedValue<float> smoothLowShelfFreq;
    juce::SmoothedValue<float> smoothLowShelfGain;
    juce::SmoothedValue<float> smoothMidFreq;
    juce::SmoothedValue<float> smoothMidGain;
    juce::SmoothedValue<float> smoothMidQ;
    juce::SmoothedValue<float> smoothHighShelfFreq;
    juce::SmoothedValue<float> smoothHighShelfGain;
};


