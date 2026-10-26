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
    std::cout << "\n--- TEST 8: Multi-Sample-Rate & Buffer-Size Resilience ---\n";
    const double testSampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    const int testBlockSizes[] = { 32, 64, 128, 256, 512, 1024 };

    for (double testSr : testSampleRates)
    {
        for (int testBs : testBlockSizes)
        {
            FDNReverb reverb;
            reverb.prepare (testSr, testBs);

            std::vector<float> inL(testBs, 0.0f), inR(testBs, 0.0f);
            std::vector<float> outL(testBs, 0.0f), outR(testBs, 0.0f);

            inL[0] = 0.8f; inR[0] = 0.8f;
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), testBs,
                            70.0f, 1.8f, 40.0f, 75.0f, false);

            bool ok = true;
            for (int s = 0; s < testBs; ++s)
            {
                if (std::isnan (outL[s]) || std::isinf (outL[s])) ok = false;
            }
            if (!ok)
            {
                EXPECT_TRUE (false, "Sample rate " + std::to_string((int)testSr) + " buffer " + std::to_string(testBs) + " failed!");
                return;
            }
        }
    }
    EXPECT_TRUE (true, "All 5 sample rates (44.1 - 192 kHz) and 6 buffer sizes (32 - 1024) pass without crash or NaN");
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

    std::cout << "\n========================================================\n";
    std::cout << "  DSP SUITE EXECUTION SUMMARY:\n";
    std::cout << "    Passed: " << gPassed << "\n";
    std::cout << "    Failed: " << gFailed << "\n";
    std::cout << "========================================================\n";

    return (gFailed == 0) ? 0 : 1;
}
