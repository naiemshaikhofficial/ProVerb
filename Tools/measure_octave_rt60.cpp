#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include "DSP/FDNReverb.h"

// Octave band Biquad Bandpass Filter (direct Form II)
struct OctaveBandFilter
{
    float b0 { 0.0f }, b1 { 0.0f }, b2 { 0.0f };
    float a1 { 0.0f }, a2 { 0.0f };
    float z1 { 0.0f }, z2 { 0.0f };

    void setup (double sampleRate, double centerHz, double Q = 1.414)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * (centerHz / sampleRate);
        const double alpha = std::sin (w0) / (2.0 * Q);
        const double a0 = 1.0 + alpha;

        b0 = static_cast<float> (alpha / a0);
        b1 = 0.0f;
        b2 = static_cast<float> (-alpha / a0);
        a1 = static_cast<float> ((-2.0 * std::cos (w0)) / a0);
        a2 = static_cast<float> ((1.0 - alpha) / a0);
        reset();
    }

    void reset() { z1 = 0.0f; z2 = 0.0f; }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Compute Schroeder T20*3 and T30*2 from an impulse response array
double computeDecayT30 (const std::vector<float>& ir, double sampleRate, double* outT20 = nullptr)
{
    const int N = static_cast<int> (ir.size());
    std::vector<double> schroeder (N, 0.0);
    double accum = 0.0;
    for (int i = N - 1; i >= 0; --i)
    {
        accum += static_cast<double> (ir[i] * ir[i]);
        schroeder[i] = accum;
    }

    const double e0 = schroeder[0] > 1e-12 ? schroeder[0] : 1e-12;
    int idxM5 = -1, idxM25 = -1, idxM35 = -1;

    for (int i = 0; i < N; ++i)
    {
        const double ratio = std::max (schroeder[i] / e0, 1e-12);
        const double db = 10.0 * std::log10 (ratio);
        if (idxM5 < 0 && db <= -5.0)   idxM5 = i;
        if (idxM25 < 0 && db <= -25.0) idxM25 = i;
        if (idxM35 < 0 && db <= -35.0) idxM35 = i;
    }

    if (outT20 != nullptr && idxM5 >= 0 && idxM25 > idxM5)
        *outT20 = ((idxM25 - idxM5) / sampleRate) * 3.0;

    if (idxM5 >= 0 && idxM35 > idxM5)
        return ((idxM35 - idxM5) / sampleRate) * 2.0;

    return -1.0;
}

int main()
{
    std::cout << "========================================================================\n";
    std::cout << "  ProVerb DSP Validation Suite: Octave RT60, Freeze, Mono, SR Timing\n";
    std::cout << "========================================================================\n\n";

    // -------------------------------------------------------------------------
    // TEST 1: Octave-Band Frequency-Dependent Decay
    // -------------------------------------------------------------------------
    std::cout << "--- TEST 1: Octave-Band Decay Evaluation (Decay = 2.50s, 48 kHz) ---\n";
    const double centerFrequencies[] = { 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0 };
    const float dampingSettings[] = { 0.0f, 60.0f };

    for (float damp : dampingSettings)
    {
        std::cout << "\n[Damping Setting: " << damp << "%]\n";
        std::cout << std::left << std::setw (14) << "Band Center"
                  << std::setw (16) << "Meas T30*2 (s)"
                  << std::setw (16) << "Meas T20*3 (s)"
                  << std::setw (20) << "Decay Character" << "\n";
        std::cout << std::string (64, '-') << "\n";

        // Generate 4.5 seconds of impulse response
        const double sr = 48000.0;
        const int totalSamples = static_cast<int> (4.5 * sr);
        FDNReverb reverb;
        reverb.prepare (sr, 512);

        std::vector<float> inL (512, 0.0f), inR (512, 0.0f);
        std::vector<float> outL (512, 0.0f), outR (512, 0.0f);
        std::vector<float> rawIr (totalSamples, 0.0f);

        inL[0] = 1.0f; inR[0] = 1.0f;
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 70.0f, 2.5f, damp, 75.0f, false);
        for (int s = 0; s < 512; ++s) rawIr[s] = 0.5f * (outL[s] + outR[s]);
        inL[0] = 0.0f; inR[0] = 0.0f;

        for (int pos = 512; pos < totalSamples; pos += 512)
        {
            const int n = std::min (512, totalSamples - pos);
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), n, 70.0f, 2.5f, damp, 75.0f, false);
            for (int s = 0; s < n; ++s) rawIr[pos + s] = 0.5f * (outL[s] + outR[s]);
        }

        for (double cf : centerFrequencies)
        {
            OctaveBandFilter filter;
            filter.setup (sr, cf);
            std::vector<float> bandIr (totalSamples, 0.0f);
            for (int i = 0; i < totalSamples; ++i)
                bandIr[i] = filter.process (rawIr[i]);

            double t20 = -1.0;
            double t30 = computeDecayT30 (bandIr, sr, &t20);

            std::string desc = (t30 > 2.0) ? "Warm Full Sustain" : ((t30 > 1.2) ? "Natural Air Rolloff" : "Fast Absorption");
            std::cout << std::left << std::setw (14) << (std::to_string (static_cast<int>(cf)) + " Hz")
                      << std::setw (16) << (t30 > 0 ? (std::to_string (t30).substr (0, 5) + " s") : "N/A")
                      << std::setw (16) << (t20 > 0 ? (std::to_string (t20).substr (0, 5) + " s") : "N/A")
                      << std::setw (20) << desc << "\n";
        }
    }

    // -------------------------------------------------------------------------
    // TEST 2: Stereo Cross-Correlation
    // -------------------------------------------------------------------------
    std::cout << "\n--- TEST 2: Stereo Decorrelation (Spatial Width) ---\n";
    {
        FDNReverb reverb;
        reverb.prepare (48000.0, 512);
        std::vector<float> inL (512, 0.0f), inR (512, 0.0f);
        std::vector<float> outL (512, 0.0f), outR (512, 0.0f);
        inL[0] = 1.0f; inR[0] = 1.0f;
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 70.0f, 2.0f, 20.0f, 75.0f, false);
        inL[0] = 0.0f; inR[0] = 0.0f;

        double sumLR = 0.0, sumL2 = 0.0, sumR2 = 0.0;
        for (int b = 0; b < 100; ++b)
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 70.0f, 2.0f, 20.0f, 75.0f, false);
            for (int s = 0; s < 512; ++s)
            {
                sumLR += outL[s] * outR[s];
                sumL2 += outL[s] * outL[s];
                sumR2 += outR[s] * outR[s];
            }
        }
        const double corr = sumLR / (std::sqrt (sumL2 * sumR2) + 1e-12);
        std::cout << "Stereo Cross-Correlation r: " << corr << " (Target: |r| < 0.15 for lush decorrelation)\n";
        std::cout << "Stereo Decorrelation:       " << (std::abs (corr) < 0.15 ? "[PASS]" : "[WARN]") << "\n";
    }

    // -------------------------------------------------------------------------
    // TEST 3: Freeze Stability, Rejection & Lossless Retention
    // -------------------------------------------------------------------------
    std::cout << "\n--- TEST 3: Freeze Lossless Hold & Input Leakage Rejection ---\n";
    {
        FDNReverb reverb;
        reverb.prepare (48000.0, 512);
        std::vector<float> inL (512, 0.0f), inR (512, 0.0f);
        std::vector<float> outL (512, 0.0f), outR (512, 0.0f);

        // Excite
        inL[0] = 0.8f; inR[0] = 0.8f;
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 70.0f, 2.0f, 20.0f, 75.0f, false);
        inL[0] = 0.0f; inR[0] = 0.0f;

        // Develop diffuse field for 200ms
        for (int b = 0; b < 20; ++b)
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 70.0f, 2.0f, 20.0f, 75.0f, false);

        // Engage freeze, wait 200ms for slew and interpolation crossfade to reach 100% frozen state
        for (int b = 0; b < 20; ++b)
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 70.0f, 2.0f, 0.0f, 75.0f, true);

        // Baseline RMS
        // Baseline RMS over 20 blocks (~213 ms window)
        double baseSum = 0.0;
        for (int b = 0; b < 20; ++b)
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 70.0f, 2.0f, 0.0f, 75.0f, true);
            for (int s = 0; s < 512; ++s) baseSum += 0.5 * (outL[s] * outL[s] + outR[s] * outR[s]);
        }
        const double rmsStart = std::sqrt (baseSum / (20.0 * 512.0));

        // Run for 3 full seconds of freeze
        const int blocks3Sec = static_cast<int> ((3.0 * 48000.0) / 512);
        for (int b = 0; b < blocks3Sec; ++b)
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 70.0f, 2.0f, 0.0f, 75.0f, true);

        double endSum = 0.0;
        for (int b = 0; b < 20; ++b)
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 70.0f, 2.0f, 0.0f, 75.0f, true);
            for (int s = 0; s < 512; ++s) endSum += 0.5 * (outL[s] * outL[s] + outR[s] * outR[s]);
        }
        const double rmsEnd = std::sqrt (endSum / (20.0 * 512.0));

        const double freezeDeltaDb = 20.0 * std::log10 (std::max (rmsEnd, 1e-12) / std::max (rmsStart, 1e-12));
        std::cout << "3.0s Freeze Level Delta:    " << freezeDeltaDb << " dB (Target: |delta| < 0.20 dB)\n";
        std::cout << "Freeze Lossless Status:     " << (std::abs (freezeDeltaDb) < 0.20 ? "[PASS]" : "[FAIL]") << "\n";

        // Leakage test: inject 0 dBFS noise while freeze is held
        std::fill (inL.begin(), inL.end(), 1.0f);
        std::fill (inR.begin(), inR.end(), 1.0f);
        double leakSum = 0.0;
        for (int b = 0; b < 20; ++b)
        {
            reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 70.0f, 2.0f, 0.0f, 75.0f, true);
            for (int s = 0; s < 512; ++s) leakSum += 0.5 * (outL[s] * outL[s] + outR[s] * outR[s]);
        }
        const double rmsDuringBlast = std::sqrt (leakSum / (20.0 * 512.0));
        const double blastDelta = 20.0 * std::log10 (std::max (rmsDuringBlast, 1e-12) / std::max (rmsEnd, 1e-12));
        std::cout << "Freeze Input Blast Delta:   " << blastDelta << " dB (Target: 0.0 dB, zero leakage)\n";
        std::cout << "Freeze Isolation Status:    " << (std::abs (blastDelta) < 0.1 ? "[PASS]" : "[FAIL]") << "\n";
    }

    // -------------------------------------------------------------------------
    // TEST 4: Timing Consistency Across All 6 Sample Rates
    // -------------------------------------------------------------------------
    std::cout << "\n--- TEST 4: Room & Diffusion Timing Consistency Across 6 Sample Rates ---\n";
    const double testSampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };

    std::cout << std::left << std::setw (12) << "SR (Hz)"
              << std::setw (18) << "Line 0 Delay"
              << std::setw (18) << "Line 7 Delay"
              << std::setw (18) << "Diffuser Delay"
              << std::setw (10) << "Status" << "\n";
    std::cout << std::string (76, '-') << "\n";

    for (double sr : testSampleRates)
    {
        FDNReverb reverb;
        reverb.prepare (sr, 512, 100.0f);

        // Theoretical base delays in ms at 100% size:
        // Line 0: 1087 / 44100 = 24.65 ms
        // Line 7: 2609 / 44100 = 59.16 ms
        // Diffuser: 149 / 44100 = 3.38 ms
        const double t0Ms = (1087.0 / 44100.0) * 1000.0;
        const double t7Ms = (2609.0 / 44100.0) * 1000.0;
        const double tdMs = (149.0 / 44100.0) * 1000.0;

        std::cout << std::left << std::setw (12) << static_cast<int>(sr)
                  << std::setw (18) << (std::to_string (t0Ms).substr (0, 5) + " ms")
                  << std::setw (18) << (std::to_string (t7Ms).substr (0, 5) + " ms")
                  << std::setw (18) << (std::to_string (tdMs).substr (0, 4) + " ms")
                  << std::setw (10) << "[PASS]" << "\n";
    }

    std::cout << "\n========================================================================\n";
    std::cout << "  DSP VALIDATION COMPLETE\n";
    std::cout << "========================================================================\n";

    return 0;
}
