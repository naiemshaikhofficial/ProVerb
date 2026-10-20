#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

class ToneEQ
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        currentSampleRate = spec.sampleRate > 8000.0 ? spec.sampleRate : 44100.0;

        // Each filter handles 1 channel internally
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

        // Perceptual logarithmic smoothing (pitch space in octaves/log2)
        smoothLowCutPitch.reset (currentSampleRate, 0.035);
        smoothHighCutPitch.reset (currentSampleRate, 0.035);
        smoothLowCutPitch.setCurrentAndTargetValue (std::log2 (80.0f));
        smoothHighCutPitch.setCurrentAndTargetValue (std::log2 (14000.0f));
    }

    void reset()
    {
        lowCutL.reset();
        lowCutR.reset();
        highCutL.reset();
        highCutR.reset();
    }

    void process (juce::AudioBuffer<float>& buffer, float lowCutHz, float highCutHz)
    {
        process (buffer, buffer.getNumSamples(), lowCutHz, highCutHz);
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamplesToProcess, float lowCutHz, float highCutHz)
    {
        // Reconciled canonical ranges matching ParameterDef.h exactly, bounded by Nyquist safety clamp
        const float nyquistSafetyHz = static_cast<float> (currentSampleRate * 0.45);
        const float safeLowCutHz  = juce::jlimit (20.0f, std::min (1000.0f, nyquistSafetyHz), lowCutHz);
        const float safeHighCutHz = juce::jlimit (1000.0f, std::min (20000.0f, nyquistSafetyHz), highCutHz);

        // Target values in logarithmic octave space for natural musical response
        smoothLowCutPitch.setTargetValue (std::log2 (safeLowCutHz));
        smoothHighCutPitch.setTargetValue (std::log2 (safeHighCutHz));

        const int numSamples = std::min (numSamplesToProcess, buffer.getNumSamples());
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0)
            return;

        juce::dsp::AudioBlock<float> fullBlock (buffer.getArrayOfWritePointers(), static_cast<size_t> (numChannels), 0, static_cast<size_t> (numSamples));

        // Process in sub-blocks of up to 32 samples for smooth parameter transitions without zippering
        constexpr int subBlockSize = 32;
        for (int offset = 0; offset < numSamples; offset += subBlockSize)
        {
            const int chunkSize = std::min (subBlockSize, numSamples - offset);
            const float curLowCut  = std::exp2 (smoothLowCutPitch.skip (chunkSize));
            const float curHighCut = std::exp2 (smoothHighCutPitch.skip (chunkSize));

            lowCutL.setCutoffFrequency (curLowCut);
            highCutL.setCutoffFrequency (curHighCut);

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
        }
    }

private:
    double currentSampleRate = 44100.0;
    using Filter = juce::dsp::StateVariableTPTFilter<float>;
    Filter lowCutL;
    Filter lowCutR;
    Filter highCutL;
    Filter highCutR;

    juce::SmoothedValue<float> smoothLowCutPitch;
    juce::SmoothedValue<float> smoothHighCutPitch;
};


