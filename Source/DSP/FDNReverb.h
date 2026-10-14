#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

/**
 * Professional 8-Line Stereo Feedback Delay Network (FDN) Reverb
 * 
 * Features:
 * - 4-stage stereo All-Pass Input Diffusion Cascade (True reflection smearing)
 * - Independent stereo decorrelation (Full L/R preservation, no mono collapse)
 * - Per-line nominal decay gain with an orthogonal Householder feedback matrix
 * - Delay and diffuser times scaled with sample rate
 * - Diffused early-energy contribution
 * - Freeze disables modulation and nonlinear feedback once fully engaged
 * - Soft saturation shapes normal-mode feedback
 */
class FDNReverb
{
public:
    static constexpr int NUM_DELAYS = 8;
    static constexpr int NUM_DIFFUSERS = 4;

    FDNReverb()
    {
        // Mutually prime base delay lengths (in samples at 44.1kHz)
        const int primeDelays[NUM_DELAYS] = { 1087, 1283, 1489, 1709, 1933, 2153, 2377, 2609 };
        for (int i = 0; i < NUM_DELAYS; ++i)
        {
            baseDelayLengths[i] = primeDelays[i];
            filterStates[i] = 0.0f;
            lfoPhases[i] = (float) i * 0.785398f;
            writePointers[i] = 0;
            smoothedFeedbackGains[i] = 0.5f;
            smoothedAlphas[i] = 0.0f;
        }

        // Stereo all-pass diffuser prime delay lengths
        const int leftDiffDelays[NUM_DIFFUSERS]       = { 149, 233, 379, 541 };
        const int rightDiffDelays[NUM_DIFFUSERS]      = { 167, 251, 397, 563 };
        const int leftTankDiffDelays[NUM_DIFFUSERS]   = { 173, 269, 401, 571 };
        const int rightTankDiffDelays[NUM_DIFFUSERS]  = { 191, 281, 419, 587 };

        for (int d = 0; d < NUM_DIFFUSERS; ++d)
        {
            baseLeftDiffLengths[d]       = leftDiffDelays[d];
            baseRightDiffLengths[d]      = rightDiffDelays[d];
            leftDiffLengths[d]           = leftDiffDelays[d];
            rightDiffLengths[d]          = rightDiffDelays[d];
            leftDiffWritePos[d]          = 0;
            rightDiffWritePos[d]         = 0;

            baseLeftTankDiffLengths[d]   = leftTankDiffDelays[d];
            baseRightTankDiffLengths[d]  = rightTankDiffDelays[d];
            leftTankDiffLengths[d]       = leftTankDiffDelays[d];
            rightTankDiffLengths[d]      = rightTankDiffDelays[d];
            leftTankDiffWritePos[d]      = 0;
            rightTankDiffWritePos[d]     = 0;
        }

        for (int i = 0; i < NUM_DELAYS; ++i)
            wanderPhases[i] = (float) i * 1.047197f; // ~60 degree phase offsets
    }

    void prepare (double sampleRate, int /*maxBlockSize*/, float initialSizePercent = 70.0f)
    {
        currentSampleRate = sampleRate > 8000.0 ? sampleRate : 44100.0;
        const float sampleRateRatio = static_cast<float> (currentSampleRate / 44100.0);

        // Allocate power-of-two rings for this sample rate and the largest supported size.
        // All memory changes happen in prepare(), never in the audio callback.
        const int maxDelaySamples = static_cast<int> (std::ceil (
            static_cast<float> (baseDelayLengths[NUM_DELAYS - 1]) * sampleRateRatio
            + 4.0f * sampleRateRatio)) + 2;
        delayBufferSize = nextPowerOfTwo (maxDelaySamples);
        delayBufferMask = delayBufferSize - 1;

        for (int i = 0; i < NUM_DELAYS; ++i)
            delayBuffers[i] = std::vector<float> (static_cast<size_t> (delayBufferSize), 0.0f);

        const int maxDiffSamples = std::max (baseRightDiffLengths[NUM_DIFFUSERS - 1],
                                             baseRightTankDiffLengths[NUM_DIFFUSERS - 1]);
        const int maxDiffusionDelay = static_cast<int> (std::ceil (
            static_cast<float> (maxDiffSamples) * sampleRateRatio)) + 2;
        diffBufferSize = nextPowerOfTwo (maxDiffusionDelay);
        diffBufferMask = diffBufferSize - 1;

        for (int d = 0; d < NUM_DIFFUSERS; ++d)
        {
            leftDiffLengths[d]      = juce::jmax (1, static_cast<int> (std::lround (baseLeftDiffLengths[d] * sampleRateRatio)));
            rightDiffLengths[d]     = juce::jmax (1, static_cast<int> (std::lround (baseRightDiffLengths[d] * sampleRateRatio)));
            leftDiffBuffers[d]      = std::vector<float> (static_cast<size_t> (diffBufferSize), 0.0f);
            rightDiffBuffers[d]     = std::vector<float> (static_cast<size_t> (diffBufferSize), 0.0f);

            leftTankDiffLengths[d]  = juce::jmax (1, static_cast<int> (std::lround (baseLeftTankDiffLengths[d] * sampleRateRatio)));
            rightTankDiffLengths[d] = juce::jmax (1, static_cast<int> (std::lround (baseRightTankDiffLengths[d] * sampleRateRatio)));
            leftTankDiffBuffers[d]  = std::vector<float> (static_cast<size_t> (diffBufferSize), 0.0f);
            rightTankDiffBuffers[d] = std::vector<float> (static_cast<size_t> (diffBufferSize), 0.0f);
        }

        reset();
        smoothedSizeScale = (initialSizePercent / 100.0f) * sampleRateRatio;
    }

    void reset()
    {
        for (int i = 0; i < NUM_DELAYS; ++i)
        {
            std::fill (delayBuffers[i].begin(), delayBuffers[i].end(), 0.0f);
            writePointers[i] = 0;
            filterStates[i] = 0.0f;
            smoothedFeedbackGains[i] = 0.0f;
            smoothedAlphas[i] = 0.0f;
        }

        for (int d = 0; d < NUM_DIFFUSERS; ++d)
        {
            std::fill (leftDiffBuffers[d].begin(), leftDiffBuffers[d].end(), 0.0f);
            std::fill (rightDiffBuffers[d].begin(), rightDiffBuffers[d].end(), 0.0f);
            leftDiffWritePos[d]      = 0;
            rightDiffWritePos[d]     = 0;

            std::fill (leftTankDiffBuffers[d].begin(), leftTankDiffBuffers[d].end(), 0.0f);
            std::fill (rightTankDiffBuffers[d].begin(), rightTankDiffBuffers[d].end(), 0.0f);
            leftTankDiffWritePos[d]  = 0;
            rightTankDiffWritePos[d] = 0;
        }

        smoothedSizeScale = 0.7f;
        smoothedDiffCoeff = 0.5f;
        freezeFade = 0.0f;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples,
                  float sizePercent, float decaySeconds, float dampingPercent,
                  float diffusionPercent, bool isFrozen,
                  float erLateBalance = 0.50f, float bassDecayScale = 1.0f,
                  const float* lateInL = nullptr, const float* lateInR = nullptr)
    {
        // Keep delay times consistent across sample rates, and hold the current geometry during Freeze.
        const float sampleRateRatio = static_cast<float> (currentSampleRate / 44100.0);
        const float targetSizeScale = isFrozen
            ? smoothedSizeScale
            : (juce::jlimit (10.0f, 100.0f, sizePercent) / 100.0f) * sampleRateRatio;

        // Precompute per-line target feedback gains and Jot absorption filter coefficients
        float targetFeedbackGains[NUM_DELAYS];
        float targetAlphas[NUM_DELAYS];
        const float safeDecay = juce::jmax (0.1f, decaySeconds);
        const float dampNorm  = juce::jlimit (0.0f, 1.0f, dampingPercent / 100.0f);

        // High-frequency decay target: 0% damping = flat decay across all bands;
        // 100% damping = highs decay steep (~7x faster) for warm vintage plate / chamber damping.
        const float highDecayRatio = juce::jlimit (0.12f, 1.0f, 1.0f - 0.88f * dampNorm);
        const float safeDecayHigh  = safeDecay * highDecayRatio;

        // Bass decay scaling (Jot frequency-dependent decay)
        const float safeBassScale  = juce::jlimit (0.4f, 2.5f, bassDecayScale);
        const float safeDecayBass  = safeDecay * safeBassScale;

        for (int i = 0; i < NUM_DELAYS; ++i)
        {
            if (isFrozen)
            {
                targetFeedbackGains[i] = 1.0f; // Lossless infinite circulation
                targetAlphas[i]        = 0.0f; // Fully transparent filter
            }
            else
            {
                const float delaySecondsActual = (static_cast<float>(baseDelayLengths[i]) * smoothedSizeScale) / static_cast<float>(currentSampleRate);
                // g_mid = 10^(-3 * tau_i / T60_mid)
                const float gMid  = std::pow (10.0f, -3.0f * (delaySecondsActual / safeDecay));
                // g_high = 10^(-3 * tau_i / T60_high)
                const float gHigh = std::pow (10.0f, -3.0f * (delaySecondsActual / safeDecayHigh));
                // g_bass = 10^(-3 * tau_i / T60_bass)
                const float gBass = std::pow (10.0f, -3.0f * (delaySecondsActual / safeDecayBass));

                const float lineGain = (safeBassScale >= 1.0f)
                    ? gMid
                    : (gMid * 0.75f + gBass * 0.25f);

                targetFeedbackGains[i] = juce::jlimit (0.0f, 0.999f, lineGain);

                // Jot's absorption filter: rho = g_high / g_mid <= 1.0
                const float rho = juce::jlimit (0.01f, 1.0f, gHigh / juce::jmax (1.0e-6f, gMid));
                // alpha = (1 - rho) / (1 + rho)
                targetAlphas[i] = juce::jlimit (0.0f, 0.90f, (1.0f - rho) / (1.0f + rho));
            }
        }

        // Householder matrix factor for N=8: 2/N = 0.25
        constexpr float householderFactor = 2.0f / static_cast<float>(NUM_DELAYS);

        // Slew coefficients (~30ms glide) and targets
        const float slewSize   = static_cast<float> (1.0 - std::exp (-1.0 / (0.035 * currentSampleRate)));
        const float slewCoeff  = static_cast<float> (1.0 - std::exp (-1.0 / (0.025 * currentSampleRate)));
        const float slewFreeze = static_cast<float> (1.0 - std::exp (-1.0 / (0.015 * currentSampleRate)));
        const float targetFreeze = isFrozen ? 1.0f : 0.0f;
        const float targetDiffCoeff = juce::jlimit (0.0f, 0.72f, (diffusionPercent / 100.0f) * 0.72f);

        for (int s = 0; s < numSamples; ++s)
        {
            // Smooth parameters sample-by-sample
            smoothedSizeScale += slewSize   * (targetSizeScale - smoothedSizeScale);
            smoothedDiffCoeff += slewCoeff  * (targetDiffCoeff - smoothedDiffCoeff);
            freezeFade        += slewFreeze * (targetFreeze    - freezeFade);
            if (isFrozen && freezeFade >= 0.999f)
                freezeFade = 1.0f;
            else if (!isFrozen && freezeFade <= 0.001f)
                freezeFade = 0.0f;

            for (int i = 0; i < NUM_DELAYS; ++i)
            {
                smoothedFeedbackGains[i] += slewCoeff * (targetFeedbackGains[i] - smoothedFeedbackGains[i]);
                smoothedAlphas[i]        += slewCoeff * (targetAlphas[i]        - smoothedAlphas[i]);
            }

            const float sampleInL = inL[s];
            const float sampleInR = inR[s];
            const float inputGate = 1.0f - freezeFade;

            // Late input stream (pre-delayed). If not provided, fallback to direct input for backwards compatibility.
            const float lateSampleL = lateInL != nullptr ? lateInL[s] : sampleInL;
            const float lateSampleR = lateInR != nullptr ? lateInR[s] : sampleInR;

            // -----------------------------------------------------------------
            // STEP 1: Early Reflections Diffusion Network (Immediate undelayed dry feed)
            // -----------------------------------------------------------------
            float diffL = sampleInL * inputGate;
            float diffR = sampleInR * inputGate;
            float stL[NUM_DIFFUSERS];
            float stR[NUM_DIFFUSERS];

            for (int d = 0; d < NUM_DIFFUSERS; ++d)
            {
                diffL = processAllpass (leftDiffBuffers[d].data(), leftDiffWritePos[d],
                                        leftDiffLengths[d], smoothedDiffCoeff, diffL);
                diffR = processAllpass (rightDiffBuffers[d].data(), rightDiffWritePos[d],
                                        rightDiffLengths[d], smoothedDiffCoeff, diffR);
                stL[d] = diffL;
                stR[d] = diffR;
            }

            // Moorer / Gardner-style natural spatial early reflection cluster with stereo cross-bounces
            const float earlyReflectL = (stL[0] * 0.28f - stR[1] * 0.22f + stL[2] * 0.18f - stR[3] * 0.14f) * inputGate;
            const float earlyReflectR = (stR[0] * 0.28f - stL[1] * 0.22f + stR[2] * 0.18f - stL[3] * 0.14f) * inputGate;

            // -----------------------------------------------------------------
            // STEP 2: Late Tank Diffusion Network (Pre-delayed stream)
            // -----------------------------------------------------------------
            float tankDiffL = lateSampleL * inputGate;
            float tankDiffR = lateSampleR * inputGate;

            for (int d = 0; d < NUM_DIFFUSERS; ++d)
            {
                tankDiffL = processAllpass (leftTankDiffBuffers[d].data(), leftTankDiffWritePos[d],
                                            leftTankDiffLengths[d], smoothedDiffCoeff, tankDiffL);
                tankDiffR = processAllpass (rightTankDiffBuffers[d].data(), rightTankDiffWritePos[d],
                                            rightTankDiffLengths[d], smoothedDiffCoeff, tankDiffR);
            }

            // ER to Late Send: seamlessly injects early diffuse energy into the reverberant tank
            const float injL = (tankDiffL + earlyReflectL * 0.18f) * inputGate;
            const float injR = (tankDiffR + earlyReflectR * 0.18f) * inputGate;

            // -----------------------------------------------------------------
            // STEP 3: Read FDN Delay Lines with Spin (Fast LFO) & Wander (Slow organic drift)
            // -----------------------------------------------------------------
            float delayOutputs[NUM_DELAYS];
            for (int i = 0; i < NUM_DELAYS; ++i)
            {
                // Spin LFO: periodic flutter prevention (0.12Hz to 0.45Hz)
                const float modRateHz = 0.12f + (0.045f * (float) i);
                lfoPhases[i] += juce::MathConstants<float>::twoPi * modRateHz / static_cast<float>(currentSampleRate);
                if (lfoPhases[i] >= juce::MathConstants<float>::twoPi)
                    lfoPhases[i] -= juce::MathConstants<float>::twoPi;

                // Wander LFO: ultra-slow organic spatial drift (0.031Hz to 0.067Hz)
                const float wanderRateHz = 0.031f + (0.005f * (float) i);
                wanderPhases[i] += juce::MathConstants<float>::twoPi * wanderRateHz / static_cast<float>(currentSampleRate);
                if (wanderPhases[i] >= juce::MathConstants<float>::twoPi)
                    wanderPhases[i] -= juce::MathConstants<float>::twoPi;

                // Both Spin and Wander fade smoothly to 0 during Freeze to maintain perfect lossless hold
                const float spinDepth   = 2.2f * sampleRateRatio * (1.0f - freezeFade);
                const float wanderDepth = 0.9f * sampleRateRatio * (1.0f - freezeFade);
                const float modOffset   = (std::sin (lfoPhases[i]) * spinDepth) + (std::sin (wanderPhases[i]) * wanderDepth);
                const float currentDelay = (static_cast<float>(baseDelayLengths[i]) * smoothedSizeScale) + modOffset;

                delayOutputs[i] = fastReadSample (i, currentDelay, freezeFade);
            }

            // -----------------------------------------------------------------
            // STEP 4: Orthogonal Householder Feedback Matrix Transform
            // -----------------------------------------------------------------
            float sumOutputs = 0.0f;
            for (int i = 0; i < NUM_DELAYS; ++i)
                sumOutputs += delayOutputs[i];

            // -----------------------------------------------------------------
            // STEP 5: Input Injection Matrix B, Damping, Line Gains
            // -----------------------------------------------------------------
            const float inputVector[NUM_DELAYS] = {
                ( injL * 0.70f + injR * 0.30f),
                ( injR * 0.70f + injL * 0.30f),
                (-injL * 0.65f + injR * 0.35f),
                (-injR * 0.65f + injL * 0.35f),
                ( injL * 0.50f - injR * 0.50f),
                (-injR * 0.50f + injL * 0.50f),
                ( injL * 0.40f + injR * 0.60f),
                (-injR * 0.40f - injL * 0.60f)
            };

            for (int i = 0; i < NUM_DELAYS; ++i)
            {
                // Householder orthogonal reflection: y = x - (2/N)*sum
                const float matrixOut = delayOutputs[i] - (householderFactor * sumOutputs);

                // Jot's frequency-dependent absorption filter
                const float effectiveAlpha = smoothedAlphas[i] * (1.0f - freezeFade);
                filterStates[i] = ((1.0f - effectiveAlpha) * matrixOut) + (effectiveAlpha * filterStates[i]);

                // Anti-denormal protection
                if (std::abs (filterStates[i]) < 1.0e-18f)
                    filterStates[i] = 0.0f;

                // Feedback signal: absorption-filtered output * line gain + stereo injection
                const float curLineGain = juce::jmap (freezeFade, smoothedFeedbackGains[i], 1.0f);
                float feedbackSignal = (filterStates[i] * curLineGain) + inputVector[i];

                // Continuous smooth rational soft-saturator
                const float satAmount = (1.0f - freezeFade) * 0.22f;
                if (satAmount > 1.0e-5f)
                {
                    const float x2 = feedbackSignal * feedbackSignal;
                    feedbackSignal = feedbackSignal / (1.0f + satAmount * x2);
                }

                fastWriteSample (i, feedbackSignal);
            }

            // -----------------------------------------------------------------
            // STEP 6: Mathematically Orthonormal Stereo Output Decoding Matrix C
            // -----------------------------------------------------------------
            constexpr float normFactor = 0.35355339f; // 1 / sqrt(8)
            const float lateL = ( delayOutputs[0] + delayOutputs[1] + delayOutputs[2] + delayOutputs[3]
                                - delayOutputs[4] - delayOutputs[5] - delayOutputs[6] - delayOutputs[7]) * normFactor;

            const float lateR = ( delayOutputs[0] - delayOutputs[1] + delayOutputs[2] - delayOutputs[3]
                                + delayOutputs[4] - delayOutputs[5] + delayOutputs[6] - delayOutputs[7]) * normFactor;

            // Configurable Early Reflections vs Late Reverb balance
            const float erNorm    = juce::jlimit (0.0f, 1.0f, erLateBalance);
            const float erGain    = erNorm * 0.44f;
            const float lateGain  = 1.0f - (erNorm * 0.30f);

            // Combine early reflections + late FDN reverb
            outL[s] = (lateL * lateGain) + (earlyReflectL * (erGain / 0.22f));
            outR[s] = (lateR * lateGain) + (earlyReflectR * (erGain / 0.22f));
        }
    }

private:
    double currentSampleRate = 44100.0;
    int delayBufferSize { 1 };
    int delayBufferMask { 0 };
    int diffBufferSize { 1 };
    int diffBufferMask { 0 };
    int baseDelayLengths[NUM_DELAYS];
    std::vector<float> delayBuffers[NUM_DELAYS];
    int writePointers[NUM_DELAYS];
    float filterStates[NUM_DELAYS];
    float lfoPhases[NUM_DELAYS];
    float wanderPhases[NUM_DELAYS];
    float smoothedFeedbackGains[NUM_DELAYS];
    float smoothedAlphas[NUM_DELAYS];

    // All-pass diffusers (Early Reflections Network)
    int baseLeftDiffLengths[NUM_DIFFUSERS];
    int baseRightDiffLengths[NUM_DIFFUSERS];
    int leftDiffLengths[NUM_DIFFUSERS];
    int rightDiffLengths[NUM_DIFFUSERS];
    std::vector<float> leftDiffBuffers[NUM_DIFFUSERS];
    std::vector<float> rightDiffBuffers[NUM_DIFFUSERS];
    int leftDiffWritePos[NUM_DIFFUSERS];
    int rightDiffWritePos[NUM_DIFFUSERS];

    // All-pass diffusers (Late Tank Network)
    int baseLeftTankDiffLengths[NUM_DIFFUSERS];
    int baseRightTankDiffLengths[NUM_DIFFUSERS];
    int leftTankDiffLengths[NUM_DIFFUSERS];
    int rightTankDiffLengths[NUM_DIFFUSERS];
    std::vector<float> leftTankDiffBuffers[NUM_DIFFUSERS];
    std::vector<float> rightTankDiffBuffers[NUM_DIFFUSERS];
    int leftTankDiffWritePos[NUM_DIFFUSERS];
    int rightTankDiffWritePos[NUM_DIFFUSERS];

    // Parameter smoothing targets
    float smoothedSizeScale { 0.70f };
    float smoothedDiffCoeff { 0.50f };
    float freezeFade        { 0.0f };

    static int nextPowerOfTwo (int value) noexcept
    {
        int result = 1;
        while (result < value)
            result <<= 1;
        return result;
    }

    inline float fastReadSample (int channel, float delayInSamples, float freezeFadeAmount)
    {
        const float readPos = static_cast<float>(writePointers[channel]) - delayInSamples;
        const int iPos = static_cast<int>(std::floor (readPos));
        const float frac = readPos - static_cast<float>(iPos);

        const uint32_t idx0 = static_cast<uint32_t> (iPos) & static_cast<uint32_t> (delayBufferMask);
        const uint32_t idx1 = (idx0 + 1u) & static_cast<uint32_t> (delayBufferMask);

        const float* buf = delayBuffers[channel].data();
        const float yLinear = buf[idx0] + frac * (buf[idx1] - buf[idx0]);

        if (freezeFadeAmount <= 0.001f)
            return yLinear;

        // In freeze, use nearest integer sample to eliminate linear interpolation lowpass loss
        const int intPos = static_cast<int>(std::round (readPos));
        const uint32_t idxInt = static_cast<uint32_t> (intPos) & static_cast<uint32_t> (delayBufferMask);
        const float yInt = buf[idxInt];

        if (freezeFadeAmount >= 0.999f)
            return yInt;

        // Smooth continuous crossfade between linear and integer read during freeze transition
        return yLinear + freezeFadeAmount * (yInt - yLinear);
    }

    inline void fastWriteSample (int channel, float value)
    {
        delayBuffers[channel][writePointers[channel]] = value;
        writePointers[channel] = (writePointers[channel] + 1) & delayBufferMask;
    }

    inline float processAllpass (float* buffer, int& writePos, int delayLen, float coeff, float inSample)
    {
        const int readPos = (writePos - delayLen + diffBufferSize) & diffBufferMask;
        const float delayed = buffer[readPos];
        const float outSample = -coeff * inSample + delayed;
        buffer[writePos] = inSample + (coeff * outSample);
        writePos = (writePos + 1) & diffBufferMask;

        return outSample;
    }
};
