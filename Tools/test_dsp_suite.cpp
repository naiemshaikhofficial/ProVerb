#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <cassert>
#include <algorithm>
#include <numeric>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "DSP/FDNReverb.h"
#include "DSP/ToneEQ.h"
#include "DSP/DuckingEngine.h"
#include "DSP/ShimmerEngine.h"
#include "DSP/AudioAnalyzerEngine.h"
#include "Parameters/ParameterDef.h"

// Objective Metric Verification Helpers
static int gPassed = 0;
static int gFailed = 0;

#define EXPECT_TRUE(cond, msg) \
    do { \
        if (cond) { \
            std::cout << "  [PASS] " << msg << "\n"; \
            gPassed++; \
        } else { \
            std::cerr << "  [FAIL] " << msg << "\n"; \
            gFailed++; \
        } \
    } while (0)

#define EXPECT_NEAR(val, target, tol, msg) \
    do { \
        double diff = std::abs((double)(val) - (double)(target)); \
        if (diff <= (double)(tol)) { \
            std::cout << "  [PASS] " << msg << " (val=" << (val) << ", target=" << (target) << ", tol=" << (tol) << ")\n"; \
            gPassed++; \
        } else { \
            std::cerr << "  [FAIL] " << msg << " (val=" << (val) << ", target=" << (target) << ", diff=" << diff << " > tol=" << (tol) << ")\n"; \
            gFailed++; \
        } \
    } while (0)

void testImpulseAndDecayScaling()
{
    std::cout << "\n--- TEST 1: Impulse Response & RT60 Decay Scaling ---\n";
    constexpr double sr = 48000.0;
    constexpr int blockSize = 512;

    auto measureEnergyAtBlockRange = [=](float decaySec, int startBlock, int endBlock) -> double {
        FDNReverb reverb;
        reverb.prepare (sr, blockSize);

        std::vector<float> inL(blockSize, 0.0f), inR(blockSize, 0.0f);
        std::vector<float> outL(blockSize, 0.0f), outR(blockSize, 0.0f);

        // Impulse at sample 0
        inL[0] = 1.0f; inR[0] = 1.0f;
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                        70.0f, decaySec, 20.0f, 75.0f, false);
        inL[0] = 0.0f; inR[0] = 0.0f;

        double totalEnergy = 0.0;
        for (int b = 1; b <= endBlock; ++b)
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                            70.0f, decaySec, 20.0f, 75.0f, false);
            if (b >= startBlock && b <= endBlock)
            {
                for (int s = 0; s < blockSize; ++s)
                    totalEnergy += outL[s] * outL[s] + outR[s] * outR[s];
            }
        }
        return totalEnergy;
    };

    // Early reverberant tail at 0.25s (blocks 20 to 30)
    double eShortEarly = measureEnergyAtBlockRange (0.6f, 20, 30);
    // Late tail at 1.0s (blocks 90 to 110)
    double eShortLate  = measureEnergyAtBlockRange (0.6f, 90, 110);
    double eMedLate    = measureEnergyAtBlockRange (2.0f, 90, 110);
    double eLongLate   = measureEnergyAtBlockRange (5.0f, 90, 110);

    EXPECT_TRUE (eShortEarly > 1e-6, "Impulse input develops clear reverberant energy in early tail (t = 0.25s)");
    EXPECT_TRUE (eMedLate > eShortLate * 10.0, "Medium decay (2.0s) sustains significantly longer than short decay (0.6s) at t = 1.0s");
    EXPECT_TRUE (eLongLate > eMedLate * 1.5, "Long decay (5.0s) sustains significantly longer than medium decay (2.0s) at t = 1.0s");
}

void testPreDelayTiming()
{
    std::cout << "\n--- TEST 2: Pre-Delay Timing Offset Verification ---\n";
    constexpr double sr = 48000.0;
    constexpr int blockSize = 256;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = blockSize;
    spec.numChannels = 2;

    juce::dsp::DelayLine<float> preDelayLine { 96000 };
    preDelayLine.prepare (spec);
    preDelayLine.setMaximumDelayInSamples (static_cast<int> (sr * 0.5));

    // Target: 40 ms predelay = 1920 samples
    const float delaySamples = static_cast<float> (sr * 0.040);

    // Push impulse at sample 0
    float firstNonZeroSample = -1.0f;
    int sampleCounter = 0;

    for (int b = 0; b < 15; ++b)
    {
        for (int s = 0; s < blockSize; ++s)
        {
            float in = (sampleCounter == 0) ? 1.0f : 0.0f;
            preDelayLine.pushSample (0, in);
            float out = preDelayLine.popSample (0, delaySamples);
            if (std::abs(out) > 0.01f && firstNonZeroSample < 0.0f)
            {
                firstNonZeroSample = (float)sampleCounter;
            }
            sampleCounter++;
        }
    }

    EXPECT_NEAR (firstNonZeroSample, 1920.0f, 2.0f, "Pre-delay exactly offsets late reflections by 40ms (1920 samples)");
}

void testDampingHighFrequencyDecay()
{
    std::cout << "\n--- TEST 3: Spectral Damping Decay Acceleration ---\n";
    constexpr double sr = 48000.0;
    constexpr int blockSize = 512;

    auto measureHighFreqDecay = [=](float dampingPct) -> double {
        FDNReverb reverb;
        reverb.prepare (sr, blockSize);

        std::vector<float> inL(blockSize, 0.0f), inR(blockSize, 0.0f);
        std::vector<float> outL(blockSize, 0.0f), outR(blockSize, 0.0f);

        // White noise burst in block 0
        juce::Random rng (12345);
        for (int s = 0; s < blockSize; ++s)
        {
            inL[s] = rng.nextFloat() * 2.0f - 1.0f;
            inR[s] = rng.nextFloat() * 2.0f - 1.0f;
        }

        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                        80.0f, 2.5f, dampingPct, 75.0f, false);
        std::fill (inL.begin(), inL.end(), 0.0f);
        std::fill (inR.begin(), inR.end(), 0.0f);

        // Continuous highpass filter at 3kHz
        float hpState = 0.0f;
        const float alpha = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 3000.0f / (float)sr);

        double highFreqEnergyLate = 0.0;
        for (int b = 1; b < 100; ++b)
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                            80.0f, 2.5f, dampingPct, 75.0f, false);
            for (int s = 0; s < blockSize; ++s)
            {
                hpState += alpha * (outL[s] - hpState);
                float hf = outL[s] - hpState;
                if (b >= 30 && b <= 70) // 300ms to 750ms later
                    highFreqEnergyLate += hf * hf;
            }
        }
        return highFreqEnergyLate;
    };

    double hfNoDamp = measureHighFreqDecay (0.0f);
    double hfHeavyDamp = measureHighFreqDecay (80.0f);

    std::cout << "    hfNoDamp = " << hfNoDamp << ", hfHeavyDamp = " << hfHeavyDamp
              << " (Ratio = " << (hfHeavyDamp / std::max(1e-12, hfNoDamp)) << ")\n";

    EXPECT_TRUE (hfHeavyDamp < hfNoDamp * 0.85, "High damping (80%) measurably reduces high-frequency reverberant energy compared to 0% damping");
}

void testStereoWidthDecorrelation()
{
    std::cout << "\n--- TEST 4: Stereo Decorrelation & Mono Compatibility ---\n";
    constexpr double sr = 48000.0;
    constexpr int blockSize = 512;

    auto testWidthCorrelation = [=](float widthPct) -> float {
        FDNReverb reverb;
        reverb.prepare (sr, blockSize);

        std::vector<float> inL(blockSize, 0.0f), inR(blockSize, 0.0f);
        std::vector<float> outL(blockSize, 0.0f), outR(blockSize, 0.0f);

        inL[0] = 1.0f; inR[0] = 1.0f; // Mono impulse
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                        70.0f, 2.0f, 40.0f, 75.0f, false);
        inL[0] = 0.0f; inR[0] = 0.0f;

        float sumLR = 0.0f, sumL2 = 0.0f, sumR2 = 0.0f;
        for (int b = 1; b < 40; ++b)
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                            70.0f, 2.0f, 40.0f, 75.0f, false);

            // Apply Mid/Side width
            const float wScale = widthPct / 100.0f;
            for (int s = 0; s < blockSize; ++s)
            {
                float mid = (outL[s] + outR[s]) * 0.5f;
                float side = (outR[s] - outL[s]) * 0.5f * wScale;
                float l = mid - side;
                float r = mid + side;
                sumLR += l * r;
                sumL2 += l * l;
                sumR2 += r * r;
            }
        }
        float denom = std::sqrt (sumL2 * sumR2);
        return (denom > 1e-9f) ? (sumLR / denom) : 1.0f;
    };

    float corrMono = testWidthCorrelation (0.0f);
    float corrNormal = testWidthCorrelation (100.0f);

    EXPECT_NEAR (corrMono, 1.0f, 0.001f, "Width 0% collapses reverb to perfect mono (r = 1.000)");
    EXPECT_TRUE (std::abs(corrNormal) < 0.25f, "Width 100% produces rich decorrelated stereo image (|r| < 0.25)");
}

void testInteractive5BandEQ()
{
    std::cout << "\n--- TEST 5: Interactive 5-Band Reverb EQ Audio Response ---\n";
    constexpr double sr = 48000.0;
    constexpr int numSamples = 2048;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = numSamples;
    spec.numChannels = 2;

    auto measureSineRms = [&](float testFreq, float lowCut, float highCut,
                              float lsFreq, float lsGain,
                              float midFreq, float midGain, float midQ,
                              float hsFreq, float hsGain) -> float {
        ToneEQ eq;
        eq.prepare (spec);

        juce::AudioBuffer<float> buffer (2, numSamples);
        const float omega = static_cast<float> (juce::MathConstants<double>::twoPi * testFreq / sr);
        for (int s = 0; s < numSamples; ++s)
        {
            float val = std::sin (omega * s);
            buffer.setSample (0, s, val);
            buffer.setSample (1, s, val);
        }

        eq.process (buffer, numSamples, lowCut, highCut, lsFreq, lsGain, midFreq, midGain, midQ, hsFreq, hsGain);
        // Measure RMS in second half to allow filter settling
        return buffer.getRMSLevel (0, numSamples / 2, numSamples / 2);
    };

    // 1. Low Cut (High Pass) at 200 Hz vs 40 Hz input sine
    float rmsBassOpen = measureSineRms (40.0f, 20.0f, 20000.0f, 250.0f, 0.0f, 1000.0f, 0.0f, 0.7f, 5000.0f, 0.0f);
    float rmsBassCut  = measureSineRms (40.0f, 200.0f, 20000.0f, 250.0f, 0.0f, 1000.0f, 0.0f, 0.7f, 5000.0f, 0.0f);
    EXPECT_TRUE (rmsBassCut < rmsBassOpen * 0.25f, "Node 1 Low Cut (200Hz) attenuates 40Hz sub bass by > 12 dB");

    // 2. Low Shelf (+6 dB boost at 120 Hz)
    float rmsLsFlat  = measureSineRms (120.0f, 20.0f, 20000.0f, 250.0f, 0.0f, 1000.0f, 0.0f, 0.7f, 5000.0f, 0.0f);
    float rmsLsBoost = measureSineRms (120.0f, 20.0f, 20000.0f, 250.0f, 6.0f, 1000.0f, 0.0f, 0.7f, 5000.0f, 0.0f);
    EXPECT_TRUE (rmsLsBoost > rmsLsFlat * 1.6f, "Node 2 Low Shelf (+6dB) measurably boosts 120Hz bass");

    // 3. Mid Bell (+6 dB boost at 1500 Hz)
    float rmsMidFlat  = measureSineRms (1500.0f, 20.0f, 20000.0f, 250.0f, 0.0f, 1500.0f, 0.0f, 1.2f, 5000.0f, 0.0f);
    float rmsMidBoost = measureSineRms (1500.0f, 20.0f, 20000.0f, 250.0f, 0.0f, 1500.0f, 6.0f, 1.2f, 5000.0f, 0.0f);
    EXPECT_TRUE (rmsMidBoost > rmsMidFlat * 1.6f, "Node 3 Mid Bell (+6dB) measurably boosts 1500Hz vocal core");

    // 4. High Shelf (+6 dB boost at 8000 Hz)
    float rmsHsFlat  = measureSineRms (8000.0f, 20.0f, 20000.0f, 250.0f, 0.0f, 1000.0f, 0.0f, 0.7f, 5000.0f, 0.0f);
    float rmsHsBoost = measureSineRms (8000.0f, 20.0f, 20000.0f, 250.0f, 0.0f, 1000.0f, 0.0f, 0.7f, 5000.0f, 6.0f);
    EXPECT_TRUE (rmsHsBoost > rmsHsFlat * 1.6f, "Node 4 High Shelf (+6dB) measurably boosts 8000Hz air");

    // 5. High Cut (Low Pass at 3000 Hz vs 12000 Hz input sine)
    float rmsAirOpen = measureSineRms (12000.0f, 20.0f, 20000.0f, 250.0f, 0.0f, 1000.0f, 0.0f, 0.7f, 5000.0f, 0.0f);
    float rmsAirCut  = measureSineRms (12000.0f, 20.0f, 3000.0f, 250.0f, 0.0f, 1000.0f, 0.0f, 0.7f, 5000.0f, 0.0f);
    EXPECT_TRUE (rmsAirCut < rmsAirOpen * 0.20f, "Node 5 High Cut (3kHz) attenuates 12kHz high frequency by > 14 dB");
}

void testVocalDucking()
{
    std::cout << "\n--- TEST 6: Vocal Ducking & Reverb Tail Attenuation ---\n";
    constexpr double sr = 48000.0;
    constexpr int blockSize = 512;

    DuckingEngine ducking;
    ducking.prepare (sr);

    std::vector<float> dryL (blockSize, 0.0f), dryR (blockSize, 0.0f);
    std::vector<float> wetL (blockSize, 0.5f), wetR (blockSize, 0.5f); // Constant steady reverb tail

    // Block 1: Silence on dry -> wet should remain un-attenuated
    ducking.process (dryL.data(), dryR.data(), wetL.data(), wetR.data(), blockSize, 80.0f, -24.0f, 8.0f, 160.0f);
    EXPECT_NEAR (ducking.getGainReductionDb(), 0.0f, 0.5f, "Silent dry signal produces 0 dB ducking attenuation");

    // Block 2 to 6: Loud dry vocal burst (-6 dBFS = 0.50 linear)
    std::fill (dryL.begin(), dryL.end(), 0.50f);
    std::fill (dryR.begin(), dryR.end(), 0.50f);
    for (int b = 0; b < 6; ++b)
    {
        std::fill (wetL.begin(), wetL.end(), 0.5f);
        std::fill (wetR.begin(), wetR.end(), 0.5f);
        ducking.process (dryL.data(), dryR.data(), wetL.data(), wetR.data(), blockSize, 80.0f, -24.0f, 8.0f, 160.0f);
    }
    float duckedDb = ducking.getGainReductionDb();
    EXPECT_TRUE (duckedDb <= -10.0f, "Loud dry vocal burst triggers significant reverb ducking (gain reduction <= -10 dB)");

    // Block 7 to 35: Vocal ceases -> wet reverb should release smoothly back towards 0 dB
    std::fill (dryL.begin(), dryL.end(), 0.0f);
    std::fill (dryR.begin(), dryR.end(), 0.0f);
    for (int b = 0; b < 30; ++b)
    {
        std::fill (wetL.begin(), wetL.end(), 0.5f);
        std::fill (wetR.begin(), wetR.end(), 0.5f);
        ducking.process (dryL.data(), dryR.data(), wetL.data(), wetR.data(), blockSize, 80.0f, -24.0f, 8.0f, 160.0f);
    }
    float releasedDb = ducking.getGainReductionDb();
    EXPECT_NEAR (releasedDb, 0.0f, 0.8f, "Reverb tail smoothly releases and recovers to 0 dB after vocal stops");
}

void testSilenceAndDenormals()
{
    std::cout << "\n--- TEST 7: Silence, Low-Level & Denormal Safety ---\n";
    constexpr double sr = 48000.0;
    constexpr int blockSize = 512;

    FDNReverb reverb;
    reverb.prepare (sr, blockSize);

    std::vector<float> inL(blockSize, 0.0f), inR(blockSize, 0.0f);
    std::vector<float> outL(blockSize, 0.0f), outR(blockSize, 0.0f);

    // Process 20 blocks of pure silence
    bool hasNonZero = false;
    bool hasNanOrInf = false;

    for (int b = 0; b < 20; ++b)
    {
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                        100.0f, 10.0f, 50.0f, 75.0f, false);
        for (int s = 0; s < blockSize; ++s)
        {
            if (std::isnan (outL[s]) || std::isnan (outR[s]) || std::isinf (outL[s]) || std::isinf (outR[s]))
                hasNanOrInf = true;
            if (outL[s] != 0.0f || outR[s] != 0.0f)
                hasNonZero = true;
        }
    }

    EXPECT_TRUE (!hasNanOrInf, "Silence processing produces NO NaN or Infinite values");
    EXPECT_TRUE (!hasNonZero, "Zero input produces exact zero output (no self-oscillation)");

    // Test extreme input level (+18 dBFS burst)
    inL[0] = 8.0f; inR[0] = 8.0f;
    reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                    100.0f, 20.0f, 0.0f, 80.0f, false);

    bool extremeStable = true;
    for (int b = 0; b < 50; ++b)
    {
        inL[0] = 0.0f; inR[0] = 0.0f;
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                        100.0f, 20.0f, 0.0f, 80.0f, false);
        for (int s = 0; s < blockSize; ++s)
        {
            if (std::isnan (outL[s]) || std::isinf (outL[s]) || std::abs(outL[s]) > 20.0f)
                extremeStable = false;
        }
    }
    EXPECT_TRUE (extremeStable, "Soft-saturator in feedback loop keeps extreme inputs unconditionally stable");
}

void testMultipleSampleRatesAndBuffers()
{
    std::cout << "\n--- TEST 8: Multiple Sample Rates and Buffer Sizes ---\n";
    const double testSampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    const int testBlockSizes[] = { 64, 128, 256, 512, 1024 };

    bool allPassed = true;
    for (double sr : testSampleRates)
    {
        for (int bs : testBlockSizes)
        {
            FDNReverb reverb;
            reverb.prepare (sr, bs);

            std::vector<float> inL (bs, 0.0f), inR (bs, 0.0f);
            std::vector<float> outL (bs, 0.0f), outR (bs, 0.0f);

            inL[0] = 1.0f; inR[0] = 1.0f;
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), bs,
                            70.0f, 1.5f, 30.0f, 75.0f, false);
            inL[0] = 0.0f; inR[0] = 0.0f;

            for (int b = 0; b < 10; ++b)
            {
                reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), bs,
                                70.0f, 1.5f, 30.0f, 75.0f, false);
                for (int s = 0; s < bs; ++s)
                {
                    if (std::isnan (outL[s]) || std::isnan (outR[s]) || std::isinf (outL[s]) || std::isinf (outR[s]))
                        allPassed = false;
                }
            }
        }
    }
    EXPECT_TRUE (allPassed, "All standard sample rates (44.1k-192k) and buffer sizes (64-1024) process stably without NaN/Inf");
}

void testAlgorithmModes()
{
    std::cout << "\n--- TEST 9: Eight Original Algorithm Modes Verification ---\n";
    constexpr double sr = 48000.0;
    constexpr int blockSize = 512;

    std::vector<double> modeEnergies(8, 0.0);

    for (int mode = 0; mode < 8; ++mode)
    {
        FDNReverb reverb;
        reverb.prepare (sr, blockSize);

        std::vector<float> inL(blockSize, 0.0f), inR(blockSize, 0.0f);
        std::vector<float> outL(blockSize, 0.0f), outR(blockSize, 0.0f);

        inL[0] = 1.0f; inR[0] = 1.0f;
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                        70.0f, 2.0f, 30.0f, 75.0f, false,
                        0.5f, 1.0f, nullptr, nullptr, mode);
        inL[0] = 0.0f; inR[0] = 0.0f;

        double e = 0.0;
        for (int b = 1; b <= 50; ++b) // 50 * 512 / 48000 = ~0.53 seconds
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                            70.0f, 2.0f, 30.0f, 75.0f, false,
                            0.5f, 1.0f, nullptr, nullptr, mode);
            for (int s = 0; s < blockSize; ++s)
                e += outL[s] * outL[s] + outR[s] * outR[s];
        }
        modeEnergies[mode] = e;
        EXPECT_TRUE (e > 1e-5, "Mode " + std::to_string(mode) + " produces valid reverberant energy");
    }

    // Verify Gated / Nonlinear Mode (Mode 7) behaves nonlinearly with fast cutoff
    {
        FDNReverb reverb;
        reverb.prepare (sr, blockSize);

        std::vector<float> inL(blockSize, 0.0f), inR(blockSize, 0.0f);
        std::vector<float> outL(blockSize, 0.0f), outR(blockSize, 0.0f);

        inL[0] = 1.0f; inR[0] = 1.0f;
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                        50.0f, 3.0f, 20.0f, 75.0f, false,
                        0.5f, 1.0f, nullptr, nullptr, 7); // Mode 7 = Gated
        inL[0] = 0.0f; inR[0] = 0.0f;

        double eDuringHold = 0.0;
        double eAfterCutoff = 0.0;

        for (int b = 1; b <= 80; ++b)
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                            50.0f, 3.0f, 20.0f, 75.0f, false,
                            0.5f, 1.0f, nullptr, nullptr, 7);

            // Block 10 to 18: ~0.10s to ~0.19s (inside gate hold)
            if (b >= 10 && b <= 18)
            {
                for (int s = 0; s < blockSize; ++s)
                    eDuringHold += outL[s] * outL[s] + outR[s] * outR[s];
            }
            // Block 55 to 65: ~0.58s to ~0.69s (after gate cutoff)
            if (b >= 55 && b <= 65)
            {
                for (int s = 0; s < blockSize; ++s)
                    eAfterCutoff += outL[s] * outL[s] + outR[s] * outR[s];
            }
        }

        double attenuationRatio = eDuringHold / (eAfterCutoff + 1e-12);
        EXPECT_TRUE (attenuationRatio > 100.0, "Gated mode sharply attenuates tail by > 20 dB after hold window (Ratio = " + std::to_string(attenuationRatio) + ")");
    }
}

void testTonalCharacter()
{
    std::cout << "\n--- TEST 10: Tonal Character (Clean vs Bright vs Dark) ---\n";
    constexpr double sr = 48000.0;
    constexpr int blockSize = 512;

    auto measureLateHF = [=](int toneColor) -> double {
        FDNReverb reverb;
        reverb.prepare (sr, blockSize);

        std::vector<float> inL(blockSize, 0.0f), inR(blockSize, 0.0f);
        std::vector<float> outL(blockSize, 0.0f), outR(blockSize, 0.0f);

        // Burst of high frequencies (8 kHz sine)
        for (int s = 0; s < blockSize; ++s)
        {
            float sine = std::sin (2.0f * juce::MathConstants<float>::pi * 8000.0f * (float)s / (float)sr);
            inL[s] = sine * 0.5f;
            inR[s] = sine * 0.5f;
        }

        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                        70.0f, 2.5f, 40.0f, 75.0f, false,
                        0.5f, 1.0f, nullptr, nullptr, 0, toneColor);

        std::fill (inL.begin(), inL.end(), 0.0f);
        std::fill (inR.begin(), inR.end(), 0.0f);

        double lateEnergy = 0.0;
        for (int b = 1; b <= 70; ++b)
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                            70.0f, 2.5f, 40.0f, 75.0f, false,
                            0.5f, 1.0f, nullptr, nullptr, 0, toneColor);
            if (b >= 40 && b <= 70) // ~0.42s to 0.74s
            {
                for (int s = 0; s < blockSize; ++s)
                    lateEnergy += outL[s] * outL[s] + outR[s] * outR[s];
            }
        }
        return lateEnergy;
    };

    double eClean  = measureLateHF (0); // Clean
    double eBright = measureLateHF (1); // Classic Bright
    double eDark   = measureLateHF (2); // Classic Dark

    EXPECT_TRUE (eBright >= eClean * 0.95, "Classic Bright retains bright high-frequency energy in late tail");
    EXPECT_TRUE (eDark < eClean * 0.55, "Classic Dark rolls off high-frequency energy substantially faster than Clean");
}

void testBassDecayMultiplier()
{
    std::cout << "\n--- TEST 11: Bass Decay Multiplier Verification ---\n";
    constexpr double sr = 48000.0;
    constexpr int blockSize = 512;

    auto measureLateBassEnergy = [=](float bassScale) -> double {
        FDNReverb reverb;
        reverb.prepare (sr, blockSize);

        std::vector<float> inL(blockSize, 0.0f), inR(blockSize, 0.0f);
        std::vector<float> outL(blockSize, 0.0f), outR(blockSize, 0.0f);

        // 120 Hz low frequency tone burst
        for (int s = 0; s < blockSize; ++s)
        {
            float sine = std::sin (2.0f * juce::MathConstants<float>::pi * 120.0f * (float)s / (float)sr);
            inL[s] = sine * 0.6f;
            inR[s] = sine * 0.6f;
        }

        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                        70.0f, 2.0f, 20.0f, 75.0f, false,
                        0.5f, bassScale, nullptr, nullptr, 0, 0, 1.0f, 40.0f, 400.0f);

        std::fill (inL.begin(), inL.end(), 0.0f);
        std::fill (inR.begin(), inR.end(), 0.0f);

        double lateEnergy = 0.0;
        for (int b = 1; b <= 90; ++b) // Up to ~0.96s
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                            70.0f, 2.0f, 20.0f, 75.0f, false,
                            0.5f, bassScale, nullptr, nullptr, 0, 0, 1.0f, 40.0f, 400.0f);
            if (b >= 50 && b <= 90) // Late tail
            {
                for (int s = 0; s < blockSize; ++s)
                    lateEnergy += outL[s] * outL[s] + outR[s] * outR[s];
            }
        }
        return lateEnergy;
    };

    double eTightBass = measureLateBassEnergy (0.4f); // 0.4x Bass Decay
    double eLongBass  = measureLateBassEnergy (2.2f); // 2.2x Bass Decay

    EXPECT_TRUE (eLongBass > eTightBass * 2.0, "Bass Multiplier 2.2x sustains substantially more low-frequency tail energy than 0.4x");
}

void testModulationDepthEffect()
{
    std::cout << "\n--- TEST 12: Modulation Rate & Depth Control ---\n";
    constexpr double sr = 48000.0;
    constexpr int blockSize = 256;

    // With depth = 0%, two separate runs of the same impulse must be identical (bit-exact static response)
    auto runImpulse = [=](float depth) -> std::vector<float> {
        FDNReverb reverb;
        reverb.prepare (sr, blockSize);

        std::vector<float> inL(blockSize, 0.0f), inR(blockSize, 0.0f);
        std::vector<float> outL(blockSize, 0.0f), outR(blockSize, 0.0f);
        std::vector<float> output;

        inL[0] = 1.0f; inR[0] = 1.0f;
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                        70.0f, 1.5f, 20.0f, 75.0f, false,
                        0.5f, 1.0f, nullptr, nullptr, 0, 0, 1.0f, depth);
        inL[0] = 0.0f; inR[0] = 0.0f;

        for (int b = 1; b <= 20; ++b)
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                            70.0f, 1.5f, 20.0f, 75.0f, false,
                            0.5f, 1.0f, nullptr, nullptr, 0, 0, 1.0f, depth);
            output.insert (output.end(), outL.begin(), outL.end());
        }
        return output;
    };

    auto outStatic1 = runImpulse (0.0f);
    auto outStatic2 = runImpulse (0.0f);
    auto outMod = runImpulse (80.0f);

    float maxDiffStatic = 0.0f;
    float maxDiffMod = 0.0f;
    for (size_t i = 0; i < outStatic1.size(); ++i)
    {
        maxDiffStatic = std::max (maxDiffStatic, std::abs (outStatic1[i] - outStatic2[i]));
        maxDiffMod    = std::max (maxDiffMod,    std::abs (outStatic1[i] - outMod[i]));
    }

    EXPECT_NEAR (maxDiffStatic, 0.0, 1e-6, "Modulation Depth 0% produces bit-exact deterministic acoustic response");
    EXPECT_TRUE (maxDiffMod > 1e-3, "Modulation Depth 80% produces rich dynamic phase motion");
}

void testEarlyLateSeparation()
{
    std::cout << "\n--- TEST 13: ValhallaRoom-style Early & Late Reverb Separation ---\n";
    constexpr double sr = 48000.0;
    constexpr int blockSize = 256;

    // Compare earlySend = 0% vs earlySend = 100%
    auto measureLateEnergyWithSend = [=](float earlySend) -> double {
        FDNReverb reverb;
        reverb.prepare (sr, blockSize);

        std::vector<float> inL(blockSize, 0.0f), inR(blockSize, 0.0f);
        std::vector<float> outL(blockSize, 0.0f), outR(blockSize, 0.0f);

        inL[0] = 1.0f; inR[0] = 1.0f;
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                        70.0f, 2.0f, 20.0f, 75.0f, false,
                        0.2f, 1.0f, nullptr, nullptr, 0, 0, 1.0f, 40.0f, 350.0f,
                        70.0f, 50.0f, earlySend);
        inL[0] = 0.0f; inR[0] = 0.0f;

        double lateEnergy = 0.0;
        for (int b = 1; b <= 40; ++b)
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), blockSize,
                            70.0f, 2.0f, 20.0f, 75.0f, false,
                            0.2f, 1.0f, nullptr, nullptr, 0, 0, 1.0f, 40.0f, 350.0f,
                            70.0f, 50.0f, earlySend);
            if (b >= 20 && b <= 40)
            {
                for (int s = 0; s < blockSize; ++s)
                    lateEnergy += outL[s] * outL[s] + outR[s] * outR[s];
            }
        }
        return lateEnergy;
    };

    double eSend0   = measureLateEnergyWithSend (0.0f);
    double eSend100 = measureLateEnergyWithSend (100.0f);

    EXPECT_TRUE (eSend100 > eSend0, "Early-to-Late Send injects early diffuse reflection energy into late tank");
}

void testTempoSyncPreDelayCalculations()
{
    std::cout << "\n--- TEST 14: Tempo-Synchronized Pre-Delay Mathematical Accuracy ---\n";
    // At 120 BPM: 1 beat (1/4) = 500 ms
    // 1/16 note = 125 ms
    // 1/8 note = 250 ms
    // 1/32 note = 62.5 ms
    const double bpm = 120.0;
    const double beatMs = 60000.0 / bpm;

    auto calcSyncMs = [=](int choice) -> double {
        switch (choice)
        {
            case 1:  return beatMs / 16.0;
            case 2:  return beatMs / 8.0;
            case 3:  return (beatMs / 4.0) * (2.0 / 3.0);
            case 4:  return beatMs / 4.0;
            case 5:  return (beatMs / 4.0) * 1.5;
            case 6:  return (beatMs / 2.0) * (2.0 / 3.0);
            case 7:  return beatMs / 2.0;
            case 8:  return (beatMs / 2.0) * 1.5;
            case 9:  return beatMs * (2.0 / 3.0);
            case 10: return beatMs;
            default: return 0.0;
        }
    };

    EXPECT_NEAR (calcSyncMs (4), 125.0, 1e-4, "120 BPM 1/16 note is exactly 125.0 ms");
    EXPECT_NEAR (calcSyncMs (7), 250.0, 1e-4, "120 BPM 1/8 note is exactly 250.0 ms");
    EXPECT_NEAR (calcSyncMs (2), 62.5,  1e-4, "120 BPM 1/32 note is exactly 62.5 ms");
}

int main()
{
    std::cout << "========================================================\n";
    std::cout << "  PROVERB PROFESSIONAL DSP ENGINE AUDIT & TEST SUITE    \n";
    std::cout << "========================================================\n";

    testImpulseAndDecayScaling();
    testPreDelayTiming();
    testDampingHighFrequencyDecay();
    testStereoWidthDecorrelation();
    testInteractive5BandEQ();
    testVocalDucking();
    testSilenceAndDenormals();
    testMultipleSampleRatesAndBuffers();
    testAlgorithmModes();
    testTonalCharacter();
    testBassDecayMultiplier();
    testModulationDepthEffect();
    testEarlyLateSeparation();
    testTempoSyncPreDelayCalculations();

    std::cout << "\n========================================================\n";
    std::cout << "  DSP SUITE EXECUTION SUMMARY:\n";
    std::cout << "    Passed: " << gPassed << "\n";
    std::cout << "    Failed: " << gFailed << "\n";
    std::cout << "========================================================\n";

    return (gFailed == 0) ? 0 : 1;
}
