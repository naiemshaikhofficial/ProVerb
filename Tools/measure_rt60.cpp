#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include "DSP/FDNReverb.h"

// Schroeder Energy Decay Curve (EDC) RT60 Estimation
// T20: Time to decay from -5 dB to -25 dB, multiplied by 3
// T30: Time to decay from -5 dB to -35 dB, multiplied by 2
struct DecayMeasurement
{
    double targetSeconds;
    double sampleRate;
    double t20Seconds;
    double t30Seconds;
    double relativeErrorT20;
    double relativeErrorT30;
    bool passed;
};

DecayMeasurement measureDecay (double sampleRate, float targetDecaySec, float sizePct = 100.0f, float dampPct = 0.0f)
{
    FDNReverb reverb;
    reverb.prepare (sampleRate, 512);

    const int totalSamples = static_cast<int> ((targetDecaySec * 1.5 + 1.0) * sampleRate);
    std::vector<float> inL (512, 0.0f);
    std::vector<float> inR (512, 0.0f);
    std::vector<float> outL (512, 0.0f);
    std::vector<float> outR (512, 0.0f);

    std::vector<double> energy (totalSamples, 0.0);

    // Block 0: Impulse
    inL[0] = 1.0f;
    inR[0] = 1.0f;
    reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512,
                    sizePct, targetDecaySec, dampPct, 70.0f, false);
    for (int s = 0; s < 512; ++s)
        energy[s] = 0.5 * (outL[s] * outL[s] + outR[s] * outR[s]);

    inL[0] = 0.0f;
    inR[0] = 0.0f;

    // Remaining blocks: Silence input, capture reverb tail
    for (int pos = 512; pos < totalSamples; pos += 512)
    {
        const int numThisBlock = std::min (512, totalSamples - pos);
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), numThisBlock,
                        sizePct, targetDecaySec, dampPct, 70.0f, false);
        for (int s = 0; s < numThisBlock; ++s)
            energy[pos + s] = 0.5 * (outL[s] * outL[s] + outR[s] * outR[s]);
    }

    // Schroeder backward integration: E(t) = \int_t^T e(tau) d tau
    std::vector<double> schroeder (totalSamples, 0.0);
    double accum = 0.0;
    for (int i = totalSamples - 1; i >= 0; --i)
    {
        accum += energy[i];
        schroeder[i] = accum;
    }

    const double e0 = schroeder[0] > 1e-12 ? schroeder[0] : 1e-12;
    std::vector<double> schroederDb (totalSamples, 0.0);
    for (int i = 0; i < totalSamples; ++i)
    {
        const double ratio = std::max (schroeder[i] / e0, 1e-12);
        schroederDb[i] = 10.0 * std::log10 (ratio);
    }

    // Find crossing points for -5 dB, -25 dB, -35 dB
    auto findCrossing = [&](double targetDb) -> int
    {
        for (int i = 0; i < totalSamples; ++i)
        {
            if (schroederDb[i] <= targetDb)
                return i;
        }
        return -1;
    };

    const int idxM5  = findCrossing (-5.0);
    const int idxM25 = findCrossing (-25.0);
    const int idxM35 = findCrossing (-35.0);

    double t20 = -1.0;
    double t30 = -1.0;

    if (idxM5 >= 0 && idxM25 > idxM5)
    {
        const double deltaSec = (idxM25 - idxM5) / sampleRate;
        t20 = deltaSec * 3.0; // T20 * 3 = T60
    }

    if (idxM5 >= 0 && idxM35 > idxM5)
    {
        const double deltaSec = (idxM35 - idxM5) / sampleRate;
        t30 = deltaSec * 2.0; // T30 * 2 = T60
    }

    const double err20 = t20 > 0 ? std::abs (t20 - targetDecaySec) / targetDecaySec : 1.0;
    const double err30 = t30 > 0 ? std::abs (t30 - targetDecaySec) / targetDecaySec : 1.0;
    const bool passed = (err20 < 0.15) || (err30 < 0.15); // Within 15% tolerance of theoretical FDN formula

    return { targetDecaySec, sampleRate, t20, t30, err20, err30, passed };
}

bool testFreezeSustain (double sampleRate)
{
    FDNReverb reverb;
    reverb.prepare (sampleRate, 512);

    std::vector<float> inL (512, 0.0f);
    std::vector<float> inR (512, 0.0f);
    std::vector<float> outL (512, 0.0f);
    std::vector<float> outR (512, 0.0f);

    // Excite tank
    inL[0] = 0.8f;
    inR[0] = 0.8f;
    reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 100.0f, 2.0f, 20.0f, 70.0f, false);
    inL[0] = 0.0f;
    inR[0] = 0.0f;

    // Run for 150ms to develop full diffuse field
    for (int b = 0; b < 15; ++b)
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 100.0f, 2.0f, 20.0f, 70.0f, false);

    // Engage freeze
    std::vector<double> blockRms;
    const int freezeBlocks = static_cast<int> ((2.0 * sampleRate) / 512); // 2.0 seconds of freeze
    for (int b = 0; b < freezeBlocks; ++b)
    {
        reverb.process (inL.data(), inR.data(), outL.data(), outR.data(), 512, 100.0f, 2.0f, 0.0f, 70.0f, true);
        double sumSq = 0.0;
        for (int s = 0; s < 512; ++s)
            sumSq += 0.5 * (outL[s] * outL[s] + outR[s] * outR[s]);
        blockRms.push_back (std::sqrt (sumSq / 512.0));
    }

    // Compare RMS at beginning of freeze (after slew ~50ms) vs 2 seconds later
    const int startIdx = static_cast<int> (0.1 * sampleRate / 512);
    const int endIdx = static_cast<int> (blockRms.size() - 2);
    const double rmsStart = blockRms[startIdx];
    const double rmsEnd   = blockRms[endIdx];

    const double dropDb = 20.0 * std::log10 (std::max (rmsEnd, 1e-12) / std::max (rmsStart, 1e-12));

    std::cout << "[Freeze Test] " << sampleRate << " Hz | RMS Start: " << rmsStart
              << " | RMS after 2.0s: " << rmsEnd << " | Level Delta: " << dropDb << " dB\n";

    // Lossless sustain criteria: Less than 1.5 dB drop over 2 full seconds of freeze (previously was dropping >10 dB)
    return std::abs (dropDb) < 1.5;
}

int main()
{
    std::cout << "========================================================================\n";
    std::cout << "  ProVerb FDN Reverb: RT60 Impulse Response & Freeze Audit\n";
    std::cout << "========================================================================\n\n";

    const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
    const float decayTargets[] = { 0.8f, 1.8f, 3.5f };

    bool allDecaysPassed = true;

    std::cout << std::left << std::setw (10) << "SR (Hz)"
              << std::setw (14) << "Target RT60"
              << std::setw (14) << "Meas T20*3"
              << std::setw (14) << "Meas T30*2"
              << std::setw (14) << "Error %"
              << std::setw (10) << "Status" << "\n";
    std::cout << std::string (76, '-') << "\n";

    for (double sr : sampleRates)
    {
        for (float target : decayTargets)
        {
            auto m = measureDecay (sr, target);
            if (!m.passed) allDecaysPassed = false;

            const double bestErr = std::min (m.relativeErrorT20, m.relativeErrorT30) * 100.0;
            std::cout << std::left << std::setw (10) << static_cast<int>(sr)
                      << std::setw (14) << (std::to_string (target).substr (0, 4) + " s")
                      << std::setw (14) << (m.t20Seconds > 0 ? (std::to_string (m.t20Seconds).substr (0, 5) + " s") : "N/A")
                      << std::setw (14) << (m.t30Seconds > 0 ? (std::to_string (m.t30Seconds).substr (0, 5) + " s") : "N/A")
                      << std::setw (14) << (std::to_string (bestErr).substr (0, 4) + " %")
                      << std::setw (10) << (m.passed ? "[PASS]" : "[FAIL]") << "\n";
        }
    }

    std::cout << "\n========================================================================\n";
    std::cout << "  Freeze Lossless Infinite Sustain Verification\n";
    std::cout << "========================================================================\n";

    bool allFreezePassed = true;
    for (double sr : sampleRates)
    {
        if (!testFreezeSustain (sr))
            allFreezePassed = false;
    }

    std::cout << "\n========================================================================\n";
    std::cout << "  Audit Summary\n";
    std::cout << "========================================================================\n";
    std::cout << "  RT60 Formula Accuracy: " << (allDecaysPassed ? "PASSED (all within tolerance)" : "FAILED") << "\n";
    std::cout << "  Freeze Infinite Hold:  " << (allFreezePassed ? "PASSED (lossless sustain verified)" : "FAILED") << "\n";

    return (allDecaysPassed && allFreezePassed) ? 0 : 1;
}
