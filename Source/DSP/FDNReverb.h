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
            bassFilterStates[i] = 0.0f;
            vintageLpStates[i] = 0.0f;
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

        // Allocate power-of-two rings for this sample rate and the largest supported room size.
        // Accommodate max roomScale = 1.85, max modeDelayScale = 1.25 (Wide Hall), plus LFO modulation excursion (2.5x total headroom).
        // All memory changes happen in prepare(), never in the audio callback.
        constexpr float maxScaleTotal = 2.50f;
        const int maxDelaySamples = static_cast<int> (std::ceil (
            static_cast<float> (baseDelayLengths[NUM_DELAYS - 1]) * maxScaleTotal * sampleRateRatio
            + 64.0f * sampleRateRatio)) + 4;
        delayBufferSize = nextPowerOfTwo (maxDelaySamples);
        delayBufferMask = delayBufferSize - 1;

        for (int i = 0; i < NUM_DELAYS; ++i)
            delayBuffers[i] = std::vector<float> (static_cast<size_t> (delayBufferSize), 0.0f);

        const int maxDiffSamples = std::max (baseRightDiffLengths[NUM_DIFFUSERS - 1],
                                             baseRightTankDiffLengths[NUM_DIFFUSERS - 1]);
        const int maxDiffusionDelay = static_cast<int> (std::ceil (
            static_cast<float> (maxDiffSamples) * 2.50f * sampleRateRatio)) + 4;
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
        const float initNorm = juce::jlimit (0.0f, 1.0f, initialSizePercent / 100.0f);
        const float initRoomScale = 0.28f + std::pow (initNorm, 1.15f) * 1.57f;
        smoothedSizeScale = initRoomScale * sampleRateRatio;
        smoothedEarlyScale = (0.32f + std::pow (initNorm, 1.10f) * 1.38f) * sampleRateRatio;
    }

    void reset()
    {
        for (int i = 0; i < NUM_DELAYS; ++i)
        {
            std::fill (delayBuffers[i].begin(), delayBuffers[i].end(), 0.0f);
            writePointers[i] = 0;
            filterStates[i] = 0.0f;
            bassFilterStates[i] = 0.0f;
            vintageLpStates[i] = 0.0f;
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
        smoothedEarlyScale = 0.7f;
        smoothedEarlyCross = 0.5f;
        smoothedEarlySend = 0.2f;
        smoothedDiffCoeff = 0.5f;
        freezeFade = 0.0f;
        smoothedHallCrossfeed = 0.0f;
        smoothedVintageLpCoeff = 0.0f;
        smoothedSatAmount = 0.22f;
        activeGateGain = 1.0f;
        gatedCounter = 0;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples,
                  float sizePercent, float decaySeconds, float dampingPercent,
                  float diffusionPercent, bool isFrozen,
                  float erLateBalance = 0.50f, float bassDecayScale = 1.0f,
                  const float* lateInL = nullptr, const float* lateInR = nullptr,
                  int algoMode = 0, int toneColor = 0,
                  float modRateHz = 1.0f, float modDepthPercent = 40.0f,
                  float bassCrossoverHz = 350.0f,
                  float earlySizePercent = 70.0f, float earlyCrossPercent = 50.0f, float earlySendPercent = 20.0f)
    {
        const float sampleRateRatio = static_cast<float> (currentSampleRate / 44100.0);

        // Algorithm Mode Characteristics:
        // 0: Neutral Room    (balanced prime scale, natural early reflections, moderate diffusion)
        // 1: Dense Room      (compact delay scale 0.82x, higher diffusion smearing)
        // 2: Wide Hall       (expanded prime scale 1.25x, asymmetric stereo taps, slow bloom)
        // 3: Plate Character (fast initial diffusion 0.82x, bright high end, iron plate saturation)
        // 4: Smooth Chamber  (golden ratio prime delays, minimum flutter, warm even decay)
        // 5: Modulated Space (expanded dual LFO drift 1.8x, cosmic ambient movement)
        // 6: Vintage Digital (10.5 kHz feedback bandwidth limiter, warm soft tank saturation)
        // 7: Gated/Nonlinear (envelope shaper: sustained energy then fast clean musical cutoff)
        float modeDelayScale = 1.0f;
        float modeDiffMax = 0.72f;
        float modeSatAmount = 0.22f;
        float modeModMult = 1.0f;

        switch (juce::jlimit (0, 7, algoMode))
        {
            case 1: // Dense Room
                modeDelayScale = 0.82f;
                modeDiffMax = 0.80f;
                modeSatAmount = 0.24f;
                break;
            case 2: // Wide Hall
                modeDelayScale = 1.25f;
                modeDiffMax = 0.65f;
                modeSatAmount = 0.20f;
                break;
            case 3: // Plate Character
                modeDelayScale = 0.94f;
                modeDiffMax = 0.84f;
                modeSatAmount = 0.28f;
                break;
            case 4: // Smooth Chamber
                modeDelayScale = 1.08f;
                modeDiffMax = 0.70f;
                modeSatAmount = 0.20f;
                break;
            case 5: // Modulated Space
                modeDelayScale = 1.15f;
                modeDiffMax = 0.70f;
                modeModMult = 1.75f;
                modeSatAmount = 0.18f;
                break;
            case 6: // Vintage Digital
                modeDelayScale = 1.0f;
                modeDiffMax = 0.68f;
                modeSatAmount = 0.35f;
                break;
            case 7: // Gated / Nonlinear
                modeDelayScale = 0.88f;
                modeDiffMax = 0.76f;
                modeSatAmount = 0.25f;
                break;
            case 0: // Neutral Room
            default:
                modeDelayScale = 1.0f;
                modeDiffMax = 0.72f;
                modeSatAmount = 0.22f;
                break;
        }

        // Tonal Character adjustments
        if (toneColor == 1) // Classic Bright (80s Sheen)
            modeSatAmount *= 0.90f;
        else if (toneColor == 2) // Classic Dark (Warm Vintage)
            modeSatAmount *= 1.45f;

        // Musical room scaling curve: 0.28x at 0% (intimate booth) to 1.85x at 100% (vast hall/cathedral)
        const float sizeNorm = juce::jlimit (0.0f, 1.0f, sizePercent / 100.0f);
        const float roomScale = 0.28f + std::pow (sizeNorm, 1.15f) * 1.57f;

        // Early reflection room scaling: 0.32x at 0% to 1.70x at 100%
        const float erRoomFactor = 0.32f + std::pow (sizeNorm, 1.10f) * 1.38f;

        // Keep delay times consistent across sample rates and hold geometry during Freeze
        const float targetSizeScale = isFrozen
            ? smoothedSizeScale
            : (roomScale * modeDelayScale) * sampleRateRatio;

        const float targetEarlyScale = isFrozen
            ? smoothedEarlyScale
            : ((juce::jlimit (10.0f, 100.0f, earlySizePercent) / 70.0f) * erRoomFactor) * sampleRateRatio;

        // Precompute per-line target feedback gains and Jot absorption filter coefficients
        float targetFeedbackGains[NUM_DELAYS];
        float targetAlphas[NUM_DELAYS];
        const float safeDecay = juce::jmax (0.1f, decaySeconds);
        const float dampNorm  = juce::jlimit (0.0f, 1.0f, dampingPercent / 100.0f);

        // High-frequency decay target:
        float highDecayBase = (algoMode == 3 || toneColor == 1) ? 0.78f : 0.88f;
        const float highDecayRatio = juce::jlimit (0.10f, 1.0f, 1.0f - highDecayBase * dampNorm);
        const float safeDecayHigh  = safeDecay * highDecayRatio;

        // Bass decay scaling (Jot frequency-dependent decay)
        const float safeBassScale  = juce::jlimit (0.2f, 3.0f, bassDecayScale);
        juce::ignoreUnused (safeDecay);

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

                targetFeedbackGains[i] = juce::jlimit (0.0f, 0.999f, gMid);

                // Jot's absorption filter: rho = g_high / g_mid <= 1.0
                float rho = juce::jlimit (0.01f, 1.0f, gHigh / juce::jmax (1.0e-6f, gMid));
                if (toneColor == 1) // Classic Bright Sheen
                    rho = juce::jmin (1.0f, rho * 1.15f);

                // alpha = (1 - rho) / (1 + rho)
                float alpha = (1.0f - rho) / (1.0f + rho);
                if (toneColor == 2) // Classic Dark
                    alpha = juce::jlimit (0.10f, 0.92f, alpha * 1.30f + 0.05f);

                targetAlphas[i] = juce::jlimit (0.0f, 0.92f, alpha);
            }
        }

        // Bass Crossover filter coefficient
        const float safeCrossoverHz = juce::jlimit (100.0f, 1500.0f, bassCrossoverHz);
        const float betaBass = std::exp (-juce::MathConstants<float>::twoPi * safeCrossoverHz / static_cast<float> (currentSampleRate));
        const float bassDelta = (safeBassScale - 1.0f) * 0.65f;

        // Slew targets for click-resistant algorithm mode transitions
        const float targetVintageLpCoeff = (toneColor == 2 || algoMode == 6)
            ? std::exp (-juce::MathConstants<float>::twoPi * 10500.0f / static_cast<float> (currentSampleRate))
            : 0.0f;
        const float targetHallCrossfeed = (algoMode == 2) ? 0.25f : 0.0f;
        const float targetSatAmount = (1.0f - freezeFade) * modeSatAmount;

        // Householder matrix factor for N=8: 2/N = 0.25
        constexpr float householderFactor = 2.0f / static_cast<float>(NUM_DELAYS);

        // Slew coefficients (~30ms glide) and targets
        const float slewSize   = static_cast<float> (1.0 - std::exp (-1.0 / (0.035 * currentSampleRate)));
        const float slewCoeff  = static_cast<float> (1.0 - std::exp (-1.0 / (0.025 * currentSampleRate)));
        const float slewFreeze = static_cast<float> (1.0 - std::exp (-1.0 / (0.015 * currentSampleRate)));
        const float targetFreeze = isFrozen ? 1.0f : 0.0f;
        const float targetDiffCoeff = juce::jlimit (0.0f, modeDiffMax, (diffusionPercent / 100.0f) * modeDiffMax);
        const float targetEarlyCross = juce::jlimit (0.0f, 1.0f, earlyCrossPercent / 100.0f);
        const float targetEarlySend  = juce::jlimit (0.0f, 1.0f, earlySendPercent / 100.0f);

        // Modulation scales
        const float safeModRate  = juce::jlimit (0.05f, 5.0f, modRateHz) * modeModMult;
        const float safeModDepth = juce::jlimit (0.0f, 2.5f, (modDepthPercent / 40.0f) * modeModMult);

        // Gated Reverb Detection & Trigger
        const int gateHoldSamples = static_cast<int> (currentSampleRate * (0.12f + (juce::jlimit (10.0f, 100.0f, sizePercent) / 100.0f) * 0.35f));
        const int gateReleaseSamples = static_cast<int> (currentSampleRate * 0.035f); // 35ms release

        if (algoMode == 7)
        {
            float blockMaxIn = 0.0f;
            for (int s = 0; s < numSamples; ++s)
            {
                blockMaxIn = std::max (blockMaxIn, std::abs (inL[s]));
                blockMaxIn = std::max (blockMaxIn, std::abs (inR[s]));
            }
            if (blockMaxIn > 0.0056f) // -45 dBFS trigger threshold
            {
                gatedCounter = 0;
            }
        }

        for (int s = 0; s < numSamples; ++s)
        {
            // Smooth parameters sample-by-sample
            smoothedSizeScale  += slewSize   * (targetSizeScale  - smoothedSizeScale);
            smoothedEarlyScale += slewSize   * (targetEarlyScale - smoothedEarlyScale);
            smoothedDiffCoeff  += slewCoeff  * (targetDiffCoeff  - smoothedDiffCoeff);
            smoothedEarlyCross += slewCoeff  * (targetEarlyCross - smoothedEarlyCross);
            smoothedEarlySend  += slewCoeff  * (targetEarlySend  - smoothedEarlySend);
            freezeFade         += slewFreeze * (targetFreeze     - freezeFade);

            smoothedHallCrossfeed  += slewCoeff * (targetHallCrossfeed  - smoothedHallCrossfeed);
            smoothedVintageLpCoeff += slewCoeff * (targetVintageLpCoeff - smoothedVintageLpCoeff);
            smoothedSatAmount      += slewCoeff * (targetSatAmount      - smoothedSatAmount);

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

            // Late input stream (pre-delayed)
            const float lateSampleL = lateInL != nullptr ? lateInL[s] : sampleInL;
            const float lateSampleR = lateInR != nullptr ? lateInR[s] : sampleInR;

            // -----------------------------------------------------------------
            // STEP 1: Early Reflections Diffusion Network (ValhallaRoom Early Size & Cross)
            // -----------------------------------------------------------------
            float diffL = sampleInL * inputGate;
            float diffR = sampleInR * inputGate;
            float stL[NUM_DIFFUSERS];
            float stR[NUM_DIFFUSERS];

            for (int d = 0; d < NUM_DIFFUSERS; ++d)
            {
                // Early diffuser lengths scale with earlySize
                const int earlyLenL = juce::jmax (1, static_cast<int> (leftDiffLengths[d] * (smoothedEarlyScale / sampleRateRatio)));
                const int earlyLenR = juce::jmax (1, static_cast<int> (rightDiffLengths[d] * (smoothedEarlyScale / sampleRateRatio)));

                diffL = processAllpass (leftDiffBuffers[d].data(), leftDiffWritePos[d],
                                        earlyLenL, smoothedDiffCoeff, diffL);
                diffR = processAllpass (rightDiffBuffers[d].data(), rightDiffWritePos[d],
                                        earlyLenR, smoothedDiffCoeff, diffR);
                stL[d] = diffL;
                stR[d] = diffR;
            }

            // Early Reflection Cluster with controllable Early Cross (stereo bleed scales with room size)
            const float cross = smoothedEarlyCross * (0.25f + sizeNorm * 0.35f);
            const float earlyReflectL = (((stL[0] * (1.0f - cross) - stR[1] * cross) * 0.28f)
                                       + ((stL[2] * (1.0f - cross) - stR[3] * cross) * 0.18f)) * inputGate;

            const float earlyReflectR = (((stR[0] * (1.0f - cross) - stL[1] * cross) * 0.28f)
                                       + ((stR[2] * (1.0f - cross) - stL[3] * cross) * 0.18f)) * inputGate;

            // -----------------------------------------------------------------
            // STEP 2: Late Tank Diffusion Network (Pre-delayed stream + Early Send)
            // -----------------------------------------------------------------
            float tankDiffL = lateSampleL * inputGate;
            float tankDiffR = lateSampleR * inputGate;

            // Late tank diffusers scale with room size: tight and dense for small rooms, blooming and spacious for large halls
            const float tankRoomFactor = 0.38f + std::pow (sizeNorm, 1.15f) * 1.12f;
            for (int d = 0; d < NUM_DIFFUSERS; ++d)
            {
                const int tankLenL = juce::jmax (1, static_cast<int> (leftTankDiffLengths[d] * tankRoomFactor));
                const int tankLenR = juce::jmax (1, static_cast<int> (rightTankDiffLengths[d] * tankRoomFactor));

                tankDiffL = processAllpass (leftTankDiffBuffers[d].data(), leftTankDiffWritePos[d],
                                            tankLenL, smoothedDiffCoeff, tankDiffL);
                tankDiffR = processAllpass (rightTankDiffBuffers[d].data(), rightTankDiffWritePos[d],
                                            tankLenR, smoothedDiffCoeff, tankDiffR);
            }

            // ER to Late Send: variable injection of early diffuse energy into late tank
            const float sendFactor = smoothedEarlySend * 0.45f;
            const float injL = (tankDiffL + earlyReflectL * sendFactor) * inputGate;
            const float injR = (tankDiffR + earlyReflectR * sendFactor) * inputGate;

            // -----------------------------------------------------------------
            // STEP 3: Read FDN Delay Lines with Mod Rate & Mod Depth
            // -----------------------------------------------------------------
            float delayOutputs[NUM_DELAYS];
            for (int i = 0; i < NUM_DELAYS; ++i)
            {
                // Spin LFO: periodic flutter prevention (scaled by modRate)
                const float modRateHzActual = (0.12f + (0.045f * (float) i)) * safeModRate;
                lfoPhases[i] += juce::MathConstants<float>::twoPi * modRateHzActual / static_cast<float>(currentSampleRate);
                if (lfoPhases[i] >= juce::MathConstants<float>::twoPi)
                    lfoPhases[i] -= juce::MathConstants<float>::twoPi;

                // Wander LFO: ultra-slow organic spatial drift (scaled by modRate)
                const float wanderRateHzActual = (0.031f + (0.005f * (float) i)) * safeModRate;
                wanderPhases[i] += juce::MathConstants<float>::twoPi * wanderRateHzActual / static_cast<float>(currentSampleRate);
                if (wanderPhases[i] >= juce::MathConstants<float>::twoPi)
                    wanderPhases[i] -= juce::MathConstants<float>::twoPi;

                // Depths scaled by modDepth and sample rate ratio, zero during Freeze
                const float spinDepth   = 2.2f * safeModDepth * sampleRateRatio * (1.0f - freezeFade);
                const float wanderDepth = 0.9f * safeModDepth * sampleRateRatio * (1.0f - freezeFade);
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
            // STEP 5: Input Injection, Frequency-Dependent Damping, Line Gains
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

                // 5A: Frequency-dependent low-frequency shelf (Bass Decay Multiplier)
                bassFilterStates[i] = ((1.0f - betaBass) * matrixOut) + (betaBass * bassFilterStates[i]);
                if (std::abs (bassFilterStates[i]) < 1.0e-18f)
                    bassFilterStates[i] = 0.0f;

                const float lowShaped = matrixOut + (bassDelta * bassFilterStates[i]);

                // 5B: Jot's frequency-dependent absorption filter for High-Frequency Damping
                const float effectiveAlpha = smoothedAlphas[i] * (1.0f - freezeFade);
                filterStates[i] = ((1.0f - effectiveAlpha) * lowShaped) + (effectiveAlpha * filterStates[i]);

                // Anti-denormal protection
                if (std::abs (filterStates[i]) < 1.0e-18f)
                    filterStates[i] = 0.0f;

                float loopSignal = filterStates[i];

                // 5C: Optional Vintage Digital 10.5 kHz feedback roll-off (smoothly slewed)
                if (smoothedVintageLpCoeff > 0.0001f)
                {
                    vintageLpStates[i] = ((1.0f - smoothedVintageLpCoeff) * loopSignal) + (smoothedVintageLpCoeff * vintageLpStates[i]);
                    loopSignal = vintageLpStates[i];
                }

                // Feedback signal: filtered output * line gain + stereo injection
                const float curLineGain = juce::jmap (freezeFade, smoothedFeedbackGains[i], 1.0f);
                float feedbackSignal = (loopSignal * curLineGain) + inputVector[i];

                // Continuous smooth rational soft-saturator (smoothly slewed mode transition)
                if (smoothedSatAmount > 1.0e-5f)
                {
                    const float x2 = feedbackSignal * feedbackSignal;
                    feedbackSignal = feedbackSignal / (1.0f + smoothedSatAmount * x2);
                }

                fastWriteSample (i, feedbackSignal);
            }

            // -----------------------------------------------------------------
            // STEP 6: Mathematically Orthonormal Stereo Output Decoding Matrix C
            // -----------------------------------------------------------------
            constexpr float normFactor = 0.35355339f; // 1 / sqrt(8)
            float lateL = ( delayOutputs[0] + delayOutputs[1] + delayOutputs[2] + delayOutputs[3]
                          - delayOutputs[4] - delayOutputs[5] - delayOutputs[6] - delayOutputs[7]) * normFactor;

            float lateR = ( delayOutputs[0] - delayOutputs[1] + delayOutputs[2] - delayOutputs[3]
                          + delayOutputs[4] - delayOutputs[5] + delayOutputs[6] - delayOutputs[7]) * normFactor;

            // In Wide Hall (Mode 2), smoothly apply stereo width decorrelation crossfeed
            if (smoothedHallCrossfeed > 0.0001f)
            {
                const float hallDiff = (lateR - lateL) * smoothedHallCrossfeed;
                lateL -= hallDiff;
                lateR += hallDiff;
            }

            // In Gated Mode (Mode 7), apply nonlinear gate envelope with click-resistant glide
            float targetGateGain = 1.0f;
            if (algoMode == 7)
            {
                gatedCounter++;
                if (gatedCounter > gateHoldSamples)
                {
                    const int relProgress = gatedCounter - gateHoldSamples;
                    if (relProgress < gateReleaseSamples)
                    {
                        const float rFrac = static_cast<float> (relProgress) / static_cast<float> (gateReleaseSamples);
                        targetGateGain = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * rFrac));
                        targetGateGain = juce::jlimit (0.01f, 1.0f, targetGateGain);
                    }
                    else
                    {
                        targetGateGain = 0.01f; // -40 dB cutoff
                    }
                }
            }
            else
            {
                gatedCounter = 0;
                targetGateGain = 1.0f;
            }

            activeGateGain += slewCoeff * (targetGateGain - activeGateGain);
            lateL *= activeGateGain;
            lateR *= activeGateGain;

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
    float bassFilterStates[NUM_DELAYS];
    float vintageLpStates[NUM_DELAYS];
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
    float smoothedSizeScale  { 0.70f };
    float smoothedEarlyScale { 0.70f };
    float smoothedEarlyCross { 0.50f };
    float smoothedEarlySend  { 0.20f };
    float smoothedDiffCoeff  { 0.50f };
    float freezeFade         { 0.0f };
    float smoothedHallCrossfeed  { 0.0f };
    float smoothedVintageLpCoeff { 0.0f };
    float smoothedSatAmount      { 0.22f };
    float activeGateGain         { 1.0f };
    int gatedCounter         { 0 };

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

        if (freezeFadeAmount >= 0.999f)
        {
            // In freeze, use nearest integer sample to eliminate high-frequency interpolation loss
            const int intPos = static_cast<int>(std::round (readPos));
            const uint32_t idxInt = static_cast<uint32_t> (intPos) & static_cast<uint32_t> (delayBufferMask);
            return delayBuffers[channel][idxInt];
        }

        const uint32_t mask = static_cast<uint32_t> (delayBufferMask);
        const uint32_t idx0 = static_cast<uint32_t> (iPos - 1) & mask;
        const uint32_t idx1 = static_cast<uint32_t> (iPos)     & mask;
        const uint32_t idx2 = static_cast<uint32_t> (iPos + 1) & mask;
        const uint32_t idx3 = static_cast<uint32_t> (iPos + 2) & mask;

        const float* buf = delayBuffers[channel].data();
        const float y0 = buf[idx0];
        const float y1 = buf[idx1];
        const float y2 = buf[idx2];
        const float y3 = buf[idx3];

        // 4-point, 3rd-order Hermite spline interpolation for click-free audiophile delay reads
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        const float yHermite = ((c3 * frac + c2) * frac + c1) * frac + c0;

        if (freezeFadeAmount <= 0.001f)
            return yHermite;

        const int intPos = static_cast<int>(std::round (readPos));
        const uint32_t idxInt = static_cast<uint32_t> (intPos) & mask;
        const float yInt = buf[idxInt];

        // Smooth continuous crossfade between Hermite and integer read during freeze transition
        return yHermite + freezeFadeAmount * (yInt - yHermite);
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
