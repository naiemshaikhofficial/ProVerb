#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <algorithm>
#include "../AI/AudioFeatureExtractor.h"

/**
 * Intelligent Real-Time Acoustic Analyzer Engine (Auto Learn)
 * 
 * Commercial-grade architecture:
 * - 100% thread-safe atomic handshake between UI thread and Audio thread
 * - Zero memory allocations in audio thread (pure POD structs & static enum identifiers)
 * - 5-Band complementary precision crossover filter-bank:
 *     Band 0: Sub (<95 Hz)
 *     Band 1: Bass / Warmth (95 Hz - 320 Hz)
 *     Band 2: Vocal Core / Guitar Body (320 Hz - 1800 Hz)
 *     Band 3: Vocal Presence / Articulation (1800 Hz - 6500 Hz)
 *     Band 4: Air & Cymbals (>6500 Hz)
 * - Zero-Crossing Rate (ZCR) tracking for harmonic vs percussive separation
 * - Relative dynamic transient detector with refractory timing
 * - Multi-feature acoustic fingerprinting (Vocals, Drums, Guitar, Piano, Bass, Ambient Pads)
 * - Industry-standard mixing engineer reverb recipes (Abbey Road filtering, dynamic ducking, pre-delay separation)
 */
class AudioAnalyzerEngine
{
public:
    enum class State : int
    {
        Idle = 0,
        Listening = 1,
        Completed = 2
    };

    enum class ProfileId : int
    {
        None = 0,
        NoAudio,
        VocalsLead,
        VocalsRap,
        VocalWarmStudioHall,
        DrumsPunchyRoom,
        GuitarKeysWarmChamber,
        PianoKeysConcertHall,
        BassCleanTightRoom,
        SustainedPadCosmicBloom
    };

    struct AcousticProfile
    {
        ProfileId id { ProfileId::None };
        float predelayMs    { 20.0f };
        float size          { 70.0f };
        float decaySeconds  { 2.5f };
        float diffusion     { 75.0f };
        float damping       { 45.0f };
        float lowCut        { 80.0f };
        float highCut       { 14000.0f };
        float ducking       { 0.0f };
        float shimmer       { 0.0f };
        float width         { 120.0f };
        float wet           { 30.0f };
    };

    static const char* getInstrumentName (ProfileId id) noexcept
    {
        switch (id)
        {
            case ProfileId::NoAudio:                return "NO AUDIO";
            case ProfileId::VocalsLead:
            case ProfileId::VocalsRap:
            case ProfileId::VocalWarmStudioHall:    return "VOCALS";
            case ProfileId::DrumsPunchyRoom:        return "DRUMS";
            case ProfileId::GuitarKeysWarmChamber:  return "GUITAR";
            case ProfileId::PianoKeysConcertHall:   return "PIANO / KEYS";
            case ProfileId::BassCleanTightRoom:     return "BASS";
            case ProfileId::SustainedPadCosmicBloom:return "SYNTH / PAD";
            default:                                return "INSTRUMENT";
        }
    }

    static const char* getProfileName (ProfileId id) noexcept
    {
        switch (id)
        {
            case ProfileId::NoAudio:
                return "No Audio (Play Track in DAW)";
            case ProfileId::VocalsLead:
                return "Vocals (Studio Lead - Auto Ducking)";
            case ProfileId::VocalsRap:
                return "Vocals (Rap & Trap - Tight Space)";
            case ProfileId::VocalWarmStudioHall:
                return "Vocals (Warm Studio Hall - Sheen)";
            case ProfileId::DrumsPunchyRoom:
                return "Drums (Punchy Studio Room)";
            case ProfileId::GuitarKeysWarmChamber:
                return "Guitar (Warm Acoustic Chamber)";
            case ProfileId::PianoKeysConcertHall:
                return "Piano & Keys (Concert Grand Hall)";
            case ProfileId::BassCleanTightRoom:
                return "Bass (Tight Acoustic Dimension)";
            case ProfileId::SustainedPadCosmicBloom:
                return "Synth & Pad (Ethereal Cosmic Bloom)";
            default:
                return "Auto Learned Profile";
        }
    }

    void prepare (double sampleRate)
    {
        currentSampleRate = juce::jmax (22050.0, sampleRate);

        // 5-Band Crossover Filter Coefficients
        // Cutoffs: 95 Hz, 320 Hz, 1800 Hz, 6500 Hz
        alpha1 = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 95.0f   / static_cast<float>(currentSampleRate));
        alpha2 = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 320.0f  / static_cast<float>(currentSampleRate));
        alpha3 = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 1800.0f / static_cast<float>(currentSampleRate));
        alpha4 = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 6500.0f / static_cast<float>(currentSampleRate));

        // Refractory period for transient detector (~50ms)
        transientRefractorySamples = static_cast<int>(currentSampleRate * 0.050);

        reset();
    }

    void reset()
    {
        state.store (State::Idle, std::memory_order_release);
        progress.store (0.0f, std::memory_order_release);
        triggerLearn.store (false, std::memory_order_release);

        samplesAccumulated = 0;
        peakLevel = 0.0f;
        rmsSum = 0.0;
        subEnergy = 0.0;
        bassEnergy = 0.0;
        vocalCoreEnergy = 0.0;
        vocalPresEnergy = 0.0;
        airEnergy = 0.0;
        zeroCrossings = 0;
        lastSample = 0.0f;

        transientHits = 0;
        samplesSinceLastTransient = 99999;

        lp1 = 0.0f;
        lp2 = 0.0f;
        lp3 = 0.0f;
        lp4 = 0.0f;
        envFast = 0.0f;
        envSlow = 0.0f;
        envVarianceSum = 0.0;
    }

    // Thread-safe trigger called from UI thread (lock-free)
    void startLearning (float durationSeconds = 2.5f)
    {
        requestedDurationSeconds.store (juce::jlimit (0.5f, 10.0f, durationSeconds), std::memory_order_release);
        triggerLearn.store (true, std::memory_order_release);
    }

    void abortLearning()
    {
        triggerLearn.store (false, std::memory_order_release);
        state.store (State::Idle, std::memory_order_release);
        progress.store (0.0f, std::memory_order_release);
    }

    bool isListening() const noexcept
    {
        return state.load (std::memory_order_acquire) == State::Listening
            || triggerLearn.load (std::memory_order_acquire);
    }

    float getProgress() const noexcept
    {
        return progress.load (std::memory_order_acquire);
    }

    void extractFeatureSnapshot (ProVerbAI::FeatureSnapshot& snapshot) const
    {
        const double meanRms = std::sqrt (rmsSum / static_cast<double>(std::max<int64_t>(1, samplesAccumulated)));
        snapshot.samplesAccumulated = samplesAccumulated;
        snapshot.sampleRate = currentSampleRate;
        snapshot.peakLevel = peakLevel;
        snapshot.meanRms = meanRms;
        snapshot.isSilence = (meanRms < 0.002);

        if (snapshot.isSilence)
        {
            snapshot.features.fill (0.0f);
            return;
        }

        const float crestFactor = (meanRms > 1.0e-5) ? (peakLevel / static_cast<float>(meanRms)) : 1.0f;
        const double totalBandEnergy = subEnergy + bassEnergy + vocalCoreEnergy + vocalPresEnergy + airEnergy + 1.0e-12;

        snapshot.features[ProVerbAI::SubRatio]       = static_cast<float>(subEnergy / totalBandEnergy);
        snapshot.features[ProVerbAI::BassRatio]      = static_cast<float>(bassEnergy / totalBandEnergy);
        snapshot.features[ProVerbAI::VocalCoreRatio] = static_cast<float>(vocalCoreEnergy / totalBandEnergy);
        snapshot.features[ProVerbAI::VocalPresRatio] = static_cast<float>(vocalPresEnergy / totalBandEnergy);
        snapshot.features[ProVerbAI::AirRatio]       = static_cast<float>(airEnergy / totalBandEnergy);
        snapshot.features[ProVerbAI::CrestFactor]    = crestFactor;
        snapshot.features[ProVerbAI::Zcr]            = static_cast<float>(zeroCrossings) / static_cast<float>(std::max<int64_t>(1, samplesAccumulated));

        const float durationSec = static_cast<float>(samplesAccumulated) / static_cast<float>(currentSampleRate);
        snapshot.features[ProVerbAI::TransientDensity] = static_cast<float>(transientHits) / std::max (0.5f, durationSec);
        snapshot.features[ProVerbAI::EnvDynamics]      = static_cast<float>(envVarianceSum * 10000.0 / static_cast<double>(std::max<int64_t>(1, samplesAccumulated)));

        snapshot.sanitize();
    }

    // Runs on Audio Thread (Zero memory allocations) - feeds AI worker thread
    bool process (const float* left, const float* right, int numSamples, ProVerbAI::FeatureSnapshot& outSnapshot)
    {
        // Handle trigger request safely on audio thread
        if (triggerLearn.exchange (false, std::memory_order_acq_rel))
        {
            state.store (State::Listening, std::memory_order_release);
            progress.store (0.0f, std::memory_order_release);
            totalAnalysisSamples = static_cast<int64_t> (currentSampleRate * requestedDurationSeconds.load (std::memory_order_acquire));
            samplesAccumulated = 0;
            peakLevel = 0.0f;
            rmsSum = 0.0;
            subEnergy = 0.0;
            bassEnergy = 0.0;
            vocalCoreEnergy = 0.0;
            vocalPresEnergy = 0.0;
            airEnergy = 0.0;
            zeroCrossings = 0;
            lastSample = 0.0f;
            transientHits = 0;
            samplesSinceLastTransient = 99999;
            lp1 = 0.0f;
            lp2 = 0.0f;
            lp3 = 0.0f;
            lp4 = 0.0f;
            envFast = 0.0f;
            envSlow = 0.0f;
            envVarianceSum = 0.0;
        }

        if (state.load (std::memory_order_acquire) != State::Listening)
            return false;

        const float fastCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.003 * currentSampleRate)));  // 3ms attack
        const float slowCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.045 * currentSampleRate))); // 45ms decay

        for (int i = 0; i < numSamples; ++i)
        {
            const float mono = (left[i] + right[i]) * 0.5f;
            const float absVal = std::abs (mono);

            peakLevel = std::max (peakLevel, absVal);
            rmsSum += mono * mono;

            // Zero-Crossing Tracking
            if ((mono >= 0.0f && lastSample < 0.0f) || (mono < 0.0f && lastSample >= 0.0f))
                zeroCrossings++;
            lastSample = mono;

            // 1. 5-Band Crossover Filter Bank
            lp1 += alpha1 * (mono - lp1);
            lp2 += alpha2 * (mono - lp2);
            lp3 += alpha3 * (mono - lp3);
            lp4 += alpha4 * (mono - lp4);

            const float bSub       = lp1;
            const float bBass      = lp2 - lp1;
            const float bVocalCore = lp3 - lp2;
            const float bVocalPres = lp4 - lp3;
            const float bAir       = mono - lp4;

            subEnergy       += bSub * bSub;
            bassEnergy      += bBass * bBass;
            vocalCoreEnergy += bVocalCore * bVocalCore;
            vocalPresEnergy += bVocalPres * bVocalPres;
            airEnergy       += bAir * bAir;

            // 2. Relative Envelope Tracking & Adaptive Transient Detection
            envFast += fastCoeff * (absVal - envFast);
            envSlow += slowCoeff * (absVal - envSlow);

            const float envDiff = envFast - envSlow;
            envVarianceSum += envDiff * envDiff;

            samplesSinceLastTransient++;
            if (samplesSinceLastTransient > transientRefractorySamples)
            {
                // Transient hit: fast envelope sharply exceeds slow envelope by 2.2x and above -36dB
                if (envFast > (envSlow * 2.2f + 0.015f) && absVal > 0.02f)
                {
                    transientHits++;
                    samplesSinceLastTransient = 0;
                }
            }

            samplesAccumulated++;
            if (samplesAccumulated >= totalAnalysisSamples)
            {
                extractFeatureSnapshot (outSnapshot);
                state.store (State::Completed, std::memory_order_release);
                progress.store (1.0f, std::memory_order_release);
                return true;
            }
        }

        if (totalAnalysisSamples > 0)
        {
            const float p = std::min (0.99f, static_cast<float>(samplesAccumulated) / static_cast<float>(totalAnalysisSamples));
            progress.store (p, std::memory_order_release);
        }

        return false;
    }

    // Backward-compatible overload
    bool process (const float* left, const float* right, int numSamples, AcousticProfile& outProfile)
    {
        ProVerbAI::FeatureSnapshot snap;
        if (process (left, right, numSamples, snap))
        {
            classifyAndGenerateProfile (outProfile);
            return true;
        }
        return false;
    }

private:
    double currentSampleRate = 44100.0;
    int64_t totalAnalysisSamples = 123480;
    int64_t samplesAccumulated = 0;
    int transientRefractorySamples = 2200;

    std::atomic<State> state { State::Idle };
    std::atomic<float> progress { 0.0f };
    std::atomic<bool> triggerLearn { false };
    std::atomic<float> requestedDurationSeconds { 2.5f };

    // Filter Coefficients & State
    float alpha1 = 0.013f;
    float alpha2 = 0.044f;
    float alpha3 = 0.23f;
    float alpha4 = 0.62f;

    float lp1 = 0.0f;
    float lp2 = 0.0f;
    float lp3 = 0.0f;
    float lp4 = 0.0f;

    float peakLevel = 0.0f;
    double rmsSum = 0.0;
    double subEnergy = 0.0;
    double bassEnergy = 0.0;
    double vocalCoreEnergy = 0.0;
    double vocalPresEnergy = 0.0;
    double airEnergy = 0.0;

    int64_t zeroCrossings = 0;
    float lastSample = 0.0f;

    int transientHits = 0;
    int samplesSinceLastTransient = 99999;

    float envFast = 0.0f;
    float envSlow = 0.0f;
    double envVarianceSum = 0.0;

    void classifyAndGenerateProfile (AcousticProfile& p)
    {
        const double meanRms = std::sqrt (rmsSum / static_cast<double>(std::max<int64_t>(1, samplesAccumulated)));

        // 1. SILENCE OR INAUDIBLE SIGNAL (< -54 dBFS)
        if (meanRms < 0.002)
        {
            p.id = ProfileId::NoAudio;
            p.predelayMs    = 20.0f;
            p.size          = 65.0f;
            p.decaySeconds  = 2.2f;
            p.diffusion     = 75.0f;
            p.damping       = 45.0f;
            p.lowCut        = 80.0f;
            p.highCut       = 14000.0f;
            p.ducking       = 0.0f;
            p.shimmer       = 0.0f;
            p.width         = 100.0f;
            p.wet           = 30.0f;
            return;
        }

        // Relative feature extraction
        const float crestFactor = (meanRms > 1.0e-5) ? (peakLevel / static_cast<float>(meanRms)) : 1.0f;
        const double totalBandEnergy = subEnergy + bassEnergy + vocalCoreEnergy + vocalPresEnergy + airEnergy + 1.0e-12;

        const float subRatio       = static_cast<float>(subEnergy / totalBandEnergy);
        const float bassRatio      = static_cast<float>(bassEnergy / totalBandEnergy);
        const float vocalCoreRatio = static_cast<float>(vocalCoreEnergy / totalBandEnergy);
        const float vocalPresRatio = static_cast<float>(vocalPresEnergy / totalBandEnergy);
        const float airRatio       = static_cast<float>(airEnergy / totalBandEnergy);

        const float vocalBandRatio = vocalCoreRatio + vocalPresRatio;
        const float zcr            = static_cast<float>(zeroCrossings) / static_cast<float>(std::max<int64_t>(1, samplesAccumulated));

        const float durationSec      = static_cast<float>(samplesAccumulated) / static_cast<float>(currentSampleRate);
        const float transientDensity = static_cast<float>(transientHits) / std::max (0.5f, durationSec);
        const float envDynamics      = static_cast<float>(envVarianceSum * 10000.0 / static_cast<double>(std::max<int64_t>(1, samplesAccumulated)));

        // ---------------------------------------------------------------------
        // ACOUSTIC FINGERPRINT CONFIDENCE SCORING
        // ---------------------------------------------------------------------

        // 1. DRUMS SCORE:
        // High crest factor, fast transient density (>1.8/s), balanced punch & top-end air
        float scoreDrums = 0.0f;
        if (crestFactor > 4.2f) scoreDrums += 35.0f;
        if (transientDensity > 1.8f) scoreDrums += 45.0f;
        if ((subRatio + airRatio) > 0.30f) scoreDrums += 20.0f;

        // 2. VOCALS SCORE:
        // Energy strongly concentrated in 320 Hz - 6500 Hz (vocalBandRatio > 0.45),
        // Very low sub-bass (<95Hz is <0.12), syllabic envelope dynamics (>0.7), moderate crest factor
        float scoreVocal = 0.0f;
        if (vocalBandRatio > 0.45f) scoreVocal += 45.0f;
        if (vocalCoreRatio > 0.28f) scoreVocal += 20.0f;
        if (subRatio < 0.12f) scoreVocal += 20.0f;
        if (envDynamics > 0.6f && transientDensity < 4.5f) scoreVocal += 15.0f;
        if (zcr >= 0.035f && zcr <= 0.20f) scoreVocal += 10.0f;

        // 3. BASS SCORE (Strict! Only true bass instruments):
        // True bass has dominant sub + low bass (>0.60), and practically ZERO air/presence (<0.08)
        float scoreBass = 0.0f;
        if ((subRatio + bassRatio) > 0.60f) scoreBass += 60.0f;
        if (vocalPresRatio < 0.10f && airRatio < 0.05f) scoreBass += 35.0f;
        if (zcr < 0.045f) scoreBass += 20.0f;

        // 4. GUITAR SCORE:
        // Plucked transients (0.8 - 3.0/s), body in 100Hz - 2kHz, low sub
        float scoreGuitar = 0.0f;
        if ((bassRatio + vocalCoreRatio) > 0.50f) scoreGuitar += 35.0f;
        if (subRatio < 0.14f) scoreGuitar += 20.0f;
        if (transientDensity >= 0.8f && transientDensity <= 3.2f) scoreGuitar += 25.0f;
        if (crestFactor >= 2.5f && crestFactor <= 5.0f) scoreGuitar += 15.0f;

        // 5. PIANO / KEYS SCORE:
        // Wide polyphony across bass, core, presence, with distinct percussive-harmonic decay
        float scorePiano = 0.0f;
        if (bassRatio > 0.15f && vocalCoreRatio > 0.20f && vocalPresRatio > 0.15f) scorePiano += 40.0f;
        if (crestFactor >= 2.6f && crestFactor <= 4.6f) scorePiano += 25.0f;
        if (transientDensity >= 0.6f && transientDensity <= 2.2f) scorePiano += 20.0f;

        // 6. SYNTH / AMBIENT PAD SCORE:
        // Very low crest factor (<2.3), very few transients (<0.6/s), sustained flat envelope
        float scorePad = 0.0f;
        if (crestFactor < 2.3f) scorePad += 45.0f;
        if (transientDensity < 0.6f) scorePad += 40.0f;
        if (envDynamics < 0.5f) scorePad += 20.0f;

        // Determine best winning acoustic category
        float maxScore = scoreDrums;
        ProfileId bestId = ProfileId::DrumsPunchyRoom;

        if (scoreVocal > maxScore)      { maxScore = scoreVocal;  bestId = ProfileId::VocalsLead; }
        if (scoreGuitar > maxScore)     { maxScore = scoreGuitar; bestId = ProfileId::GuitarKeysWarmChamber; }
        if (scorePiano > maxScore)      { maxScore = scorePiano;  bestId = ProfileId::PianoKeysConcertHall; }
        if (scoreBass > maxScore)       { maxScore = scoreBass;   bestId = ProfileId::BassCleanTightRoom; }
        if (scorePad > maxScore)        { maxScore = scorePad;    bestId = ProfileId::SustainedPadCosmicBloom; }

        // ---------------------------------------------------------------------
        // APPLY INDUSTRY BEST-PRACTICE MIXING REVERB PRESETS
        // ---------------------------------------------------------------------
        if (bestId == ProfileId::VocalsLead)
        {
            if (transientDensity > 2.0f || envDynamics > 1.8f)
            {
                // Rapid modern rap/trap vocal: tight space, high ducking for clean lyric articulation
                p.id = ProfileId::VocalsRap;
                p.predelayMs    = 15.0f;
                p.size          = 48.0f;
                p.decaySeconds  = 1.10f;
                p.diffusion     = 84.0f;
                p.damping       = 38.0f;
                p.lowCut        = 150.0f;
                p.highCut       = 14000.0f;
                p.ducking       = 45.0f;
                p.shimmer       = 0.0f;
                p.width         = 130.0f;
                p.wet           = 26.0f;
            }
            else if (airRatio > 0.18f)
            {
                // Silky vocal plate with airy sheen
                p.id = ProfileId::VocalWarmStudioHall;
                p.predelayMs    = 30.0f;
                p.size          = 68.0f;
                p.decaySeconds  = 2.10f;
                p.diffusion     = 90.0f;
                p.damping       = 28.0f;
                p.lowCut        = 125.0f;
                p.highCut       = 16000.0f;
                p.ducking       = 35.0f;
                p.shimmer       = 6.0f;
                p.width         = 140.0f;
                p.wet           = 30.0f;
            }
            else
            {
                // Industry standard Lead Vocal: 28ms predelay, Abbey Road EQ, dynamic ducking
                p.id = ProfileId::VocalsLead;
                p.predelayMs    = 28.0f;
                p.size          = 62.0f;
                p.decaySeconds  = 1.85f;
                p.diffusion     = 88.0f;
                p.damping       = 35.0f;
                p.lowCut        = 130.0f;   // Abbey Road filter trick (clean vocal low-end)
                p.highCut       = 12500.0f; // Abbey Road filter trick (tames sibilant harshness)
                p.ducking       = 35.0f;    // Reverb blooms between vocal phrases!
                p.shimmer       = 0.0f;
                p.width         = 135.0f;
                p.wet           = 28.0f;
            }
        }
        else if (bestId == ProfileId::DrumsPunchyRoom)
        {
            // Industry standard Drum Room: short decay, high diffusion, tight predelay, cut low sub
            p.id = ProfileId::DrumsPunchyRoom;
            p.predelayMs    = 8.0f;
            p.size          = 38.0f;
            p.decaySeconds  = 0.75f;
            p.diffusion     = 92.0f;
            p.damping       = 52.0f;
            p.lowCut        = 180.0f;   // Protect kick drum punch from low-end reverb clutter
            p.highCut       = 10000.0f;
            p.ducking       = 0.0f;
            p.shimmer       = 0.0f;
            p.width         = 110.0f;
            p.wet           = 22.0f;
        }
        else if (bestId == ProfileId::GuitarKeysWarmChamber)
        {
            // Acoustic / Electric Guitar: warm chamber, natural wood bloom
            p.id = ProfileId::GuitarKeysWarmChamber;
            p.predelayMs    = 18.0f;
            p.size          = 58.0f;
            p.decaySeconds  = 1.55f;
            p.diffusion     = 82.0f;
            p.damping       = 40.0f;
            p.lowCut        = 100.0f;
            p.highCut       = 14000.0f;
            p.ducking       = 15.0f;
            p.shimmer       = 0.0f;
            p.width         = 125.0f;
            p.wet           = 26.0f;
        }
        else if (bestId == ProfileId::PianoKeysConcertHall)
        {
            // Concert Grand Piano: lush grand hall, wide stereo diffusion
            p.id = ProfileId::PianoKeysConcertHall;
            p.predelayMs    = 24.0f;
            p.size          = 76.0f;
            p.decaySeconds  = 2.30f;
            p.diffusion     = 88.0f;
            p.damping       = 30.0f;
            p.lowCut        = 85.0f;
            p.highCut       = 16000.0f;
            p.ducking       = 10.0f;
            p.shimmer       = 0.0f;
            p.width         = 140.0f;
            p.wet           = 28.0f;
        }
        else if (bestId == ProfileId::BassCleanTightRoom)
        {
            // Bass Instrument: ultra-short tight space, heavy low-cut to keep sub mono & clean
            p.id = ProfileId::BassCleanTightRoom;
            p.predelayMs    = 5.0f;
            p.size          = 26.0f;
            p.decaySeconds  = 0.40f;
            p.diffusion     = 65.0f;
            p.damping       = 70.0f;
            p.lowCut        = 240.0f;   // Sub remains 100% dry; only harmonic mid-range gets dimension
            p.highCut       = 6500.0f;
            p.ducking       = 30.0f;
            p.shimmer       = 0.0f;
            p.width         = 85.0f;
            p.wet           = 14.0f;
        }
        else
        {
            // Synth & Ambient Pads: ethereal cosmic bloom with shimmer
            p.id = ProfileId::SustainedPadCosmicBloom;
            p.predelayMs    = 45.0f;
            p.size          = 88.0f;
            p.decaySeconds  = 4.20f;
            p.diffusion     = 95.0f;
            p.damping       = 22.0f;
            p.lowCut        = 80.0f;
            p.highCut       = 18000.0f;
            p.ducking       = 10.0f;
            p.shimmer       = 18.0f;
            p.width         = 175.0f;
            p.wet           = 36.0f;
        }

        // Clamp all parameters strictly within APVTS definitions
        p.predelayMs   = juce::jlimit (0.0f, 300.0f, p.predelayMs);
        p.size         = juce::jlimit (10.0f, 100.0f, p.size);
        p.decaySeconds = juce::jlimit (0.1f, 30.0f, p.decaySeconds);
        p.diffusion    = juce::jlimit (0.0f, 100.0f, p.diffusion);
        p.damping      = juce::jlimit (0.0f, 100.0f, p.damping);
        p.lowCut       = juce::jlimit (20.0f, 1000.0f, p.lowCut);
        p.highCut      = juce::jlimit (1000.0f, 20000.0f, p.highCut);
        p.ducking      = juce::jlimit (0.0f, 100.0f, p.ducking);
        p.shimmer      = juce::jlimit (0.0f, 100.0f, p.shimmer);
        p.width        = juce::jlimit (0.0f, 200.0f, p.width);
        p.wet          = juce::jlimit (0.0f, 100.0f, p.wet);
    }
};
