#pragma once

#include <juce_core/juce_core.h>
#include "AudioFeatureExtractor.h"
#include "../DSP/AudioAnalyzerEngine.h"
#include <array>
#include <cmath>

/**
 * ReverbDecisionEngine
 * 
 * Intelligent decision layer between AI model outputs and ProVerb's FDN Reverb DSP.
 * 
 * Features:
 * - Translates 7-class probability distributions into studio reverb recipes.
 * - Multi-class awareness & confidence scoring.
 * - Dynamically adapts reverb parameters (decay, predelay, ducking, EQ, width) based on acoustic nuances.
 * - Respects APVTS ranges strictly.
 * - Provides verified heuristic fallback when AI model is not installed.
 */
class ReverbDecisionEngine
{
public:
    struct Recommendation
    {
        AudioAnalyzerEngine::AcousticProfile profile;
        juce::String instrumentCategory;
        juce::String profileName;
        float confidence { 0.0f };
        bool isAIModel { false };
    };

private:
    static void clampToAPVTS (AudioAnalyzerEngine::AcousticProfile& p)
    {
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

public:

    static Recommendation decideFromModel (const std::array<float, ProVerbAI::numClasses>& probs,
                                           const ProVerbAI::FeatureSnapshot& snapshot)
    {
        Recommendation rec;
        rec.isAIModel = true;

        if (snapshot.isSilence || snapshot.meanRms < 0.002)
        {
            rec.profile.id = AudioAnalyzerEngine::ProfileId::NoAudio;
            rec.instrumentCategory = "NO AUDIO";
            rec.profileName = "No Audio (Play Track in DAW)";
            rec.confidence = 0.0f;
            return rec;
        }

        // 1. Sanitize and validate probability distribution
        std::array<float, ProVerbAI::numClasses> validProbs = probs;
        float probSum = 0.0f;
        bool hasInvalid = false;
        for (auto pVal : validProbs)
        {
            if (!std::isfinite (pVal) || pVal < 0.0f)
                hasInvalid = true;
            else
                probSum += pVal;
        }

        if (hasInvalid || probSum < 1.0e-5f)
        {
            validProbs.fill (1.0f / static_cast<float>(ProVerbAI::numClasses));
        }
        else if (std::abs (probSum - 1.0f) > 0.01f)
        {
            for (auto& pVal : validProbs)
                pVal /= probSum;
        }

        // Find top class and runner-up
        int topClass = 0;
        float topProb = validProbs[0];
        int secondClass = 0;
        float secondProb = 0.0f;

        for (int i = 1; i < static_cast<int>(ProVerbAI::numClasses); ++i)
        {
            if (validProbs[i] > topProb)
            {
                secondProb = topProb;
                secondClass = topClass;
                topProb = validProbs[i];
                topClass = i;
            }
            else if (validProbs[i] > secondProb)
            {
                secondProb = validProbs[i];
                secondClass = i;
            }
        }

        const float margin = topProb - secondProb;
        rec.confidence = juce::jlimit (0.0f, 1.0f, topProb * (0.85f + 0.15f * margin));

        auto& p = rec.profile;

        // If confidence is low or top prediction is Other/Uncertain, do not force an incorrect instrument
        if (rec.confidence < 0.25f || (topClass == ProVerbAI::ClassOtherUncertain && rec.confidence < 0.45f))
        {
            rec.instrumentCategory = "UNCERTAIN / MIX";
            rec.profile.id = AudioAnalyzerEngine::ProfileId::None;
            rec.profileName = "Studio Ambience (Balanced Room - Safe)";
            p.predelayMs   = 20.0f;
            p.size         = 60.0f;
            p.decaySeconds = 1.80f;
            p.diffusion    = 80.0f;
            p.damping      = 40.0f;
            p.lowCut       = 100.0f;
            p.highCut      = 14000.0f;
            p.ducking      = 15.0f;
            p.shimmer      = 0.0f;
            p.width        = 120.0f;
            p.wet          = 25.0f;
            clampToAPVTS (p);
            return rec;
        }

        // ---------------------------------------------------------------------
        // ACOUSTIC REALITY CHECK & SENSOR FUSION DISAMBIGUATION
        // ---------------------------------------------------------------------
        const float subRatio   = snapshot.features[ProVerbAI::SubRatio];
        const float bassRatio  = snapshot.features[ProVerbAI::BassRatio];
        const float coreRatio  = snapshot.features[ProVerbAI::VocalCoreRatio];
        const float presRatio  = snapshot.features[ProVerbAI::VocalPresRatio];
        const float airRatio   = snapshot.features[ProVerbAI::AirRatio];
        const float crest      = snapshot.features[ProVerbAI::CrestFactor];
        const float zcr        = snapshot.features[ProVerbAI::Zcr];
        const float transDens  = snapshot.features[ProVerbAI::TransientDensity];
        const float envDyn     = snapshot.features[ProVerbAI::EnvDynamics];

        // 1. PLUCKED TONAL INSTRUMENTS (Acoustic / Electric Guitar, Sitar, Plucks):
        // Plucked strings exhibit sharp dynamic pick attacks (high crest factor >= 4.0),
        // rhythmic transient strikes (transDens 0.7 - 5.5 hits/s), low zero-crossing rate
        // (zcr <= 0.052 indicating resonant harmonic pitch rather than drum noise),
        // and solid acoustic wood resonance ((bassRatio + coreRatio) >= 0.38 with subRatio < 0.35).
        // NEVER hijack real guitars into Drums!
        const bool isPluckedTonal = (crest >= 4.0f && transDens >= 0.7f && transDens <= 5.5f
                                     && zcr <= 0.052f && (bassRatio + coreRatio) >= 0.38f && subRatio < 0.35f);

        if (isPluckedTonal)
        {
            if (topClass != ProVerbAI::ClassGuitar)
            {
                topClass = ProVerbAI::ClassGuitar;
                rec.confidence = std::max (rec.confidence, 0.85f);
            }
        }
        // 2. VOCALS REALITY CHECK:
        // Singing human voice is concentrated in vocal core & presence formants.
        // It CANNOT physically consist of >45% sub-bass (<95Hz) with <12% vocal core!
        // If a track has >45% sub-bass and high rhythmic transients, it is Drums/Beat (or Bass).
        else if (topClass == ProVerbAI::ClassVocals && (subRatio > 0.45f || coreRatio < 0.12f || (transDens > 5.0f && crest > 4.5f)))
        {
            if (transDens >= 1.5f || (subRatio + airRatio) > 0.30f)
                topClass = ProVerbAI::ClassDrums;
            else
                topClass = ProVerbAI::ClassBass;
            rec.confidence = std::max (rec.confidence, 0.82f);
        }
        // 3. TRUE DRUMS / PERCUSSION DISAMBIGUATION:
        // True percussion requires sharp attacks AND either substantial sub kick or high unpitched
        // noise / cymbal / snare sizzle (high ZCR or airRatio), with lower continuous harmonic body.
        else if (crest >= 3.8f && transDens >= 1.2f && (zcr >= 0.075f || subRatio >= 0.40f || airRatio >= 0.14f)
                 && (bassRatio + coreRatio) < 0.62f)
        {
            if (topClass != ProVerbAI::ClassDrums && topClass != ProVerbAI::ClassGuitar)
            {
                topClass = ProVerbAI::ClassDrums;
                rec.confidence = std::max (rec.confidence, 0.86f);
            }
        }
        // 4. TRUE BASS & 808 SUB DISAMBIGUATION:
        // Dominant energy concentrated strictly in <320Hz (>= 0.68) with negligible air/presence.
        else if ((subRatio + bassRatio) >= 0.68f && presRatio < 0.06f && airRatio < 0.03f)
        {
            if (topClass != ProVerbAI::ClassBass)
            {
                topClass = ProVerbAI::ClassBass;
                rec.confidence = std::max (rec.confidence, 0.90f);
            }
        }
        // 5. VOCAL FORMANTS & SINGING VOICE DISAMBIGUATION:
        // Human singing voice is heavily concentrated in the vocal core and presence formants
        // (coreRatio + presRatio >= 0.65), with dynamic crest inflections (crest >= 3.5),
        // and low sub-bass (subRatio < 0.15).
        // NEVER misclassify human singing vocals as Synth/Pad!
        else if ((coreRatio + presRatio) >= 0.65f && subRatio < 0.15f && crest >= 3.5f)
        {
            if (topClass != ProVerbAI::ClassVocals && topClass != ProVerbAI::ClassGuitar)
            {
                topClass = ProVerbAI::ClassVocals;
                rec.confidence = std::max (rec.confidence, 0.88f);
            }
        }

        // ---------------------------------------------------------------------
        // DYNAMIC STUDIO PARAMETER SYNTHESIS (Grammy-Level Acoustic Tailoring)
        // ---------------------------------------------------------------------
        switch (topClass)
        {
            case ProVerbAI::ClassVocals:
            {
                // Dynamic Vocal Space Calculation:
                // Pre-delay: Higher crest / transient attacks need more pre-delay (22ms - 46ms)
                // so consonants ('p', 't', 'k', 's') punch through dry before reverb blooms!
                p.predelayMs = juce::jlimit (16.0f, 48.0f, 18.0f + crest * 3.2f + (transDens > 3.0f ? 2.0f : 8.0f));

                // Decay Time:
                // Rhythmic / syllabic vocal melody (transDens > 2.8 hits/s or envDyn > 2.5) -> tight (0.85s - 1.25s)
                // Sustained singing -> lush hall (1.70s - 2.50s)
                if (transDens > 2.8f || envDyn > 2.5f)
                {
                    p.id = AudioAnalyzerEngine::ProfileId::VocalsRap;
                    rec.instrumentCategory = "VOCAL MELODY (AI)";
                    rec.profileName = "Vocal Melody (Punchy & Articulate - AI)";
                    p.decaySeconds = juce::jlimit (0.85f, 1.25f, 1.30f - 0.07f * transDens);
                    p.size = juce::jlimit (42.0f, 60.0f, 46.0f + envDyn * 2.0f);
                    p.diffusion = 85.0f;
                    p.damping = 40.0f;
                    p.ducking = juce::jlimit (35.0f, 60.0f, 25.0f + envDyn * 7.5f);
                    p.wet = 18.0f;
                    p.width = 130.0f;
                }
                else if (airRatio > 0.14f && bassRatio < 0.20f)
                {
                    p.id = AudioAnalyzerEngine::ProfileId::VocalsFemaleAiry;
                    rec.instrumentCategory = "FEMALE VOCALS (AI)";
                    rec.profileName = "Female Vocal (Silky Air Plate - AI)";
                    p.decaySeconds = juce::jlimit (1.80f, 2.60f, 1.95f + airRatio * 3.5f);
                    p.size = 68.0f;
                    p.diffusion = 92.0f;
                    p.damping = 26.0f;
                    p.ducking = juce::jlimit (25.0f, 45.0f, 30.0f + envDyn * 3.0f);
                    p.shimmer = 6.0f;
                    p.wet = 20.0f;
                    p.width = 140.0f;
                }
                else
                {
                    p.id = AudioAnalyzerEngine::ProfileId::VocalsMaleWarm;
                    rec.instrumentCategory = "VOCALS (AI)";
                    rec.profileName = "Lead Vocal (Abbey Road Space - AI)";
                    p.decaySeconds = juce::jlimit (1.40f, 2.20f, 1.80f - (transDens > 1.5f ? 0.3f : 0.0f));
                    p.size = 62.0f;
                    p.diffusion = 88.0f;
                    p.damping = 34.0f;
                    p.ducking = juce::jlimit (28.0f, 50.0f, 32.0f + envDyn * 4.0f);
                    p.shimmer = 0.0f;
                    p.wet = 19.0f;
                    p.width = 130.0f;
                }

                // Dynamic Low-Cut & High-Cut:
                // High-pass filter protects the vocal from chest boom or low rumble:
                p.lowCut  = juce::jlimit (120.0f, 280.0f, 130.0f + 200.0f * (subRatio * 1.2f + bassRatio * 0.5f));
                p.highCut = juce::jlimit (9000.0f, 16000.0f, 11000.0f + 18000.0f * airRatio);
                break;
            }

            case ProVerbAI::ClassDrums:
            {
                rec.instrumentCategory = "DRUMS (AI)";

                // Kick / 808 Sub Drums
                if (subRatio > 0.35f || (subRatio > 0.22f && crest > 4.5f))
                {
                    p.id = AudioAnalyzerEngine::ProfileId::DrumsKickLowClean;
                    rec.profileName = "Kick & Sub (Tight Low-End Clean - AI)";
                    p.predelayMs   = juce::jlimit (4.0f, 12.0f, 4.0f + crest * 1.1f);
                    p.size         = 28.0f;
                    p.decaySeconds = juce::jlimit (0.35f, 0.70f, 0.60f - 0.04f * transDens);
                    p.diffusion    = 82.0f;
                    p.damping      = 65.0f;
                    p.lowCut       = juce::jlimit (200.0f, 340.0f, 220.0f + subRatio * 140.0f);
                    p.highCut      = 8500.0f;
                    p.ducking      = 0.0f;
                    p.shimmer      = 0.0f;
                    p.width        = 95.0f;
                    p.wet          = 12.0f;
                }
                // Tabla / Dholak / Indian Hand Percussion
                else if (bassRatio > 0.25f && coreRatio > 0.20f && crest < 5.2f)
                {
                    p.id = AudioAnalyzerEngine::ProfileId::PercussionTablaHand;
                    rec.profileName = "Tabla & Indian Percussion (Studio Chamber - AI)";
                    p.predelayMs   = juce::jlimit (8.0f, 18.0f, 8.0f + crest * 1.5f);
                    p.size         = 46.0f;
                    p.decaySeconds = juce::jlimit (0.80f, 1.30f, 1.20f - 0.06f * transDens);
                    p.diffusion    = 88.0f;
                    p.damping      = 44.0f;
                    p.lowCut       = juce::jlimit (120.0f, 220.0f, 130.0f + subRatio * 150.0f);
                    p.highCut      = 12000.0f;
                    p.ducking      = 10.0f;
                    p.shimmer      = 0.0f;
                    p.width        = 120.0f;
                    p.wet          = 18.0f;
                }
                // Snare / Trap Drums / Full Kit
                else
                {
                    p.id = AudioAnalyzerEngine::ProfileId::DrumsPunchyRoom;
                    rec.profileName = "Drums & Snare (Punchy Studio Room - AI)";
                    p.predelayMs   = juce::jlimit (6.0f, 16.0f, 6.0f + crest * 1.5f);
                    p.size         = 38.0f;
                    p.decaySeconds = juce::jlimit (0.60f, 1.10f, 0.95f - 0.05f * transDens);
                    p.diffusion    = 92.0f;
                    p.damping      = 50.0f;
                    p.lowCut       = juce::jlimit (140.0f, 240.0f, 160.0f + subRatio * 120.0f);
                    p.highCut      = 10500.0f;
                    p.ducking      = 0.0f;
                    p.shimmer      = 0.0f;
                    p.width        = 115.0f;
                    p.wet          = 17.0f;
                }
                break;
            }

            case ProVerbAI::ClassGuitar:
            {
                rec.instrumentCategory = "STRINGS / GUITAR (AI)";

                if (transDens >= 2.5f && (airRatio + presRatio) > 0.22f)
                {
                    p.id = AudioAnalyzerEngine::ProfileId::StringsPluckedSitar;
                    rec.profileName = "Sitar & Plucked Strings (Resonant Shimmer - AI)";
                    p.predelayMs   = juce::jlimit (16.0f, 32.0f, 18.0f + crest * 2.0f);
                    p.size         = 64.0f;
                    p.decaySeconds = juce::jlimit (1.60f, 2.50f, 2.10f - 0.08f * transDens);
                    p.diffusion    = 88.0f;
                    p.damping      = 32.0f;
                    p.lowCut       = juce::jlimit (130.0f, 220.0f, 140.0f + subRatio * 120.0f);
                    p.highCut      = 13500.0f;
                    p.ducking      = 18.0f;
                    p.shimmer      = 10.0f;
                    p.width        = 135.0f;
                    p.wet          = 18.0f;
                }
                else
                {
                    p.id = AudioAnalyzerEngine::ProfileId::GuitarKeysWarmChamber;
                    rec.profileName = "Acoustic Guitar (Warm Woody Chamber - AI)";
                    p.predelayMs   = juce::jlimit (12.0f, 26.0f, 14.0f + crest * 1.5f);
                    p.size         = 52.0f;
                    p.decaySeconds = juce::jlimit (1.00f, 1.80f, 1.45f - 0.08f * transDens);
                    p.diffusion    = 80.0f;
                    p.damping      = 45.0f;
                    p.lowCut       = juce::jlimit (120.0f, 200.0f, 130.0f + subRatio * 100.0f);
                    p.highCut      = juce::jlimit (9500.0f, 14000.0f, 11000.0f + airRatio * 12000.0f);
                    p.ducking      = 12.0f;
                    p.shimmer      = 0.0f;
                    p.width        = 120.0f;
                    p.wet          = 16.0f;
                }
                break;
            }

            case ProVerbAI::ClassPiano:
            {
                rec.instrumentCategory = "PIANO / KEYS (AI)";
                p.id = AudioAnalyzerEngine::ProfileId::PianoKeysConcertHall;
                rec.profileName = "Piano & Keys (Concert Grand Hall - AI)";
                p.predelayMs   = juce::jlimit (18.0f, 36.0f, 20.0f + crest * 1.5f);
                p.size         = 74.0f;
                p.decaySeconds = juce::jlimit (1.60f, 2.80f, 2.20f - 0.10f * transDens);
                p.diffusion    = 88.0f;
                p.damping      = 32.0f;
                p.lowCut       = juce::jlimit (80.0f, 160.0f, 95.0f + subRatio * 80.0f);
                p.highCut      = 15000.0f;
                p.ducking      = 10.0f;
                p.shimmer      = 0.0f;
                p.width        = 135.0f;
                p.wet          = 22.0f;
                break;
            }

            case ProVerbAI::ClassBass:
            {
                rec.instrumentCategory = "BASS (AI)";
                p.id = AudioAnalyzerEngine::ProfileId::BassCleanTightRoom;
                rec.profileName = "Bass & 808 (Tight Dimension - Clean Sub - AI)";
                p.predelayMs   = juce::jlimit (3.0f, 8.0f, 3.0f + crest * 0.8f);
                p.size         = 24.0f;
                p.decaySeconds = juce::jlimit (0.30f, 0.55f, 0.45f - 0.03f * transDens);
                p.diffusion    = 65.0f;
                p.damping      = 70.0f;
                p.lowCut       = juce::jlimit (220.0f, 360.0f, 240.0f + subRatio * 140.0f);
                p.highCut      = 5500.0f;
                p.ducking      = 30.0f;
                p.shimmer      = 0.0f;
                p.width        = 85.0f;
                p.wet          = 12.0f;
                break;
            }

            case ProVerbAI::ClassSynthPad:
            {
                rec.instrumentCategory = "SYNTH / PAD (AI)";
                p.id = AudioAnalyzerEngine::ProfileId::SustainedPadCosmicBloom;
                rec.profileName = "Synth & Pad (Ethereal Cosmic Bloom - AI)";
                p.predelayMs   = juce::jlimit (25.0f, 55.0f, 30.0f + crest * 3.0f);
                p.size         = 88.0f;
                p.decaySeconds = juce::jlimit (2.50f, 4.80f, 3.80f - 0.15f * transDens);
                p.diffusion    = 95.0f;
                p.damping      = 24.0f;
                p.lowCut       = juce::jlimit (70.0f, 160.0f, 80.0f + subRatio * 100.0f);
                p.highCut      = juce::jlimit (14000.0f, 19000.0f, 16000.0f + airRatio * 15000.0f);
                p.ducking      = juce::jlimit (5.0f, 20.0f, 8.0f + envDyn * 3.0f);
                p.shimmer      = 16.0f;
                p.width        = 170.0f;
                p.wet          = 28.0f;
                break;
            }

            default: // Other / Woodwinds / Mixed
            {
                if (coreRatio > 0.25f && crest < 3.5f && transDens < 1.8f && airRatio > 0.05f && subRatio < 0.12f)
                {
                    // Woodwinds & Flute (Bansuri, Shakuhachi, Flute)
                    rec.instrumentCategory = "WOODWINDS / FLUTE (AI)";
                    p.id = AudioAnalyzerEngine::ProfileId::WoodwindsFlute;
                    rec.profileName = "Flute & Bansuri (Breath of Sky - AI)";
                    p.predelayMs   = 35.0f;
                    p.size         = 82.0f;
                    p.decaySeconds = 2.80f;
                    p.diffusion    = 92.0f;
                    p.damping      = 28.0f;
                    p.lowCut       = 220.0f;
                    p.highCut      = 14500.0f;
                    p.ducking      = 15.0f;
                    p.shimmer      = 6.0f;
                    p.width        = 150.0f;
                    p.wet          = 22.0f;
                }
                else
                {
                    rec.instrumentCategory = "MIXED / AMBIENCE";
                    p.id = AudioAnalyzerEngine::ProfileId::None;
                    rec.profileName = "Studio Ambience (Balanced Mix)";
                    p.predelayMs   = 20.0f;
                    p.size         = 60.0f;
                    p.decaySeconds = 1.80f;
                    p.diffusion    = 80.0f;
                    p.damping      = 40.0f;
                    p.lowCut       = 100.0f;
                    p.highCut      = 14000.0f;
                    p.ducking      = 15.0f;
                    p.shimmer      = 0.0f;
                    p.width        = 120.0f;
                    p.wet          = 22.0f;
                }
                break;
            }
        }

        clampToAPVTS (p);
        return rec;
    }

    static Recommendation decideFromHeuristics (const ProVerbAI::FeatureSnapshot& snapshot)
    {
        Recommendation rec;
        rec.isAIModel = false;

        if (snapshot.isSilence || snapshot.meanRms < 0.002)
        {
            rec.profile.id = AudioAnalyzerEngine::ProfileId::NoAudio;
            rec.instrumentCategory = "NO AUDIO";
            rec.profileName = "No Audio (Play Track in DAW)";
            rec.confidence = 0.0f;
            return rec;
        }

        const float crestFactor      = snapshot.features[ProVerbAI::CrestFactor];
        const float subRatio         = snapshot.features[ProVerbAI::SubRatio];
        const float bassRatio        = snapshot.features[ProVerbAI::BassRatio];
        const float vocalCoreRatio   = snapshot.features[ProVerbAI::VocalCoreRatio];
        const float vocalPresRatio   = snapshot.features[ProVerbAI::VocalPresRatio];
        const float airRatio         = snapshot.features[ProVerbAI::AirRatio];
        const float vocalBandRatio   = vocalCoreRatio + vocalPresRatio;
        const float zcr              = snapshot.features[ProVerbAI::Zcr];
        const float transientDensity = snapshot.features[ProVerbAI::TransientDensity];
        const float envDynamics      = snapshot.features[ProVerbAI::EnvDynamics];

        // 1. DRUMS SCORE:
        // High crest factor (>3.0), distinct transient density (>=0.7/s for half-time/90BPM beats), punch in sub/bass and air
        float scoreDrums = 0.0f;
        if (crestFactor > 3.0f) scoreDrums += 25.0f;
        if (crestFactor > 4.2f) scoreDrums += 15.0f;
        if (transientDensity >= 0.7f) scoreDrums += 25.0f;
        if (transientDensity >= 1.5f) scoreDrums += 20.0f;
        if ((subRatio + airRatio) > 0.22f) scoreDrums += 20.0f;
        if (crestFactor > 3.6f && (subRatio > 0.14f || airRatio > 0.10f)) scoreDrums += 15.0f;

        // 2. VOCALS SCORE:
        // Energy concentrated in 320 Hz - 6500 Hz (vocalBandRatio > 0.45),
        // Very low sub-bass (<95Hz is <0.12), syllabic envelope dynamics (>0.6), moderate crest factor
        // Suppress vocal if audio exhibits plucked string behaviour (sharp plucked transients + high crest factor)
        float scoreVocal = 0.0f;
        if (vocalBandRatio > 0.45f) scoreVocal += 35.0f;
        if (vocalCoreRatio > 0.28f) scoreVocal += 15.0f;
        if (subRatio < 0.12f) scoreVocal += 15.0f;
        if (envDynamics > 0.6f && transientDensity < 4.5f) scoreVocal += 15.0f;
        if (zcr >= 0.035f && zcr <= 0.20f) scoreVocal += 10.0f;
        if (transientDensity >= 0.7f && crestFactor >= 2.8f) scoreVocal -= 25.0f;

        // 3. BASS SCORE (Strict! Only true bass instruments):
        // True bass has dominant sub + low bass (>0.60), and practically ZERO air/presence (<0.08)
        float scoreBass = 0.0f;
        if ((subRatio + bassRatio) > 0.60f) scoreBass += 60.0f;
        if (vocalPresRatio < 0.10f && airRatio < 0.05f) scoreBass += 35.0f;
        if (zcr < 0.045f) scoreBass += 20.0f;

        // 4. GUITAR SCORE:
        // Plucked note transients (0.7 - 3.8/s), acoustic body in 100Hz - 2.5kHz, low sub
        float scoreGuitar = 0.0f;
        if ((bassRatio + vocalCoreRatio) > 0.42f) scoreGuitar += 35.0f;
        if (subRatio < 0.14f) scoreGuitar += 20.0f;
        if (transientDensity >= 0.7f && transientDensity <= 3.8f) scoreGuitar += 30.0f;
        if (crestFactor >= 2.4f && crestFactor <= 5.5f) scoreGuitar += 25.0f;
        if (vocalPresRatio > 0.10f && airRatio > 0.03f) scoreGuitar += 15.0f;

        // 5. PIANO / KEYS SCORE:
        // Wide polyphony across bass, core, presence, with distinct percussive-harmonic decay
        float scorePiano = 0.0f;
        if (bassRatio > 0.15f && vocalCoreRatio > 0.20f && vocalPresRatio > 0.15f) scorePiano += 40.0f;
        if (crestFactor >= 2.6f && crestFactor <= 4.6f) scorePiano += 25.0f;
        if (transientDensity >= 0.5f && transientDensity <= 2.2f) scorePiano += 20.0f;

        // 6. SYNTH / AMBIENT PAD SCORE:
        // Very low crest factor (<2.3), very few transients (<0.6/s), sustained flat envelope
        float scorePad = 0.0f;
        if (crestFactor < 2.3f) scorePad += 45.0f;
        if (transientDensity < 0.6f) scorePad += 40.0f;
        if (envDynamics < 0.5f) scorePad += 20.0f;

        float maxScore = scoreDrums;
        AudioAnalyzerEngine::ProfileId bestId = AudioAnalyzerEngine::ProfileId::DrumsPunchyRoom;

        if (scoreVocal > maxScore)  { maxScore = scoreVocal;  bestId = AudioAnalyzerEngine::ProfileId::VocalsLead; }
        if (scoreGuitar > maxScore) { maxScore = scoreGuitar; bestId = AudioAnalyzerEngine::ProfileId::GuitarKeysWarmChamber; }
        if (scorePiano > maxScore)  { maxScore = scorePiano;  bestId = AudioAnalyzerEngine::ProfileId::PianoKeysConcertHall; }
        if (scoreBass > maxScore)   { maxScore = scoreBass;   bestId = AudioAnalyzerEngine::ProfileId::BassCleanTightRoom; }
        if (scorePad > maxScore)    { maxScore = scorePad;    bestId = AudioAnalyzerEngine::ProfileId::SustainedPadCosmicBloom; }

        auto& p = rec.profile;
        p.id = bestId;

        if (bestId == AudioAnalyzerEngine::ProfileId::VocalsLead)
        {
            rec.instrumentCategory = "VOCALS";
            if (transientDensity > 2.0f || envDynamics > 1.8f)
            {
                p.id = AudioAnalyzerEngine::ProfileId::VocalsRap;
                rec.profileName = "Vocals (Rap & Trap - Tight Space)";
                p.predelayMs   = 15.0f; p.size = 48.0f; p.decaySeconds = 1.10f; p.diffusion = 84.0f; p.damping = 38.0f;
                p.lowCut = 150.0f; p.highCut = 14000.0f; p.ducking = 45.0f; p.shimmer = 0.0f; p.width = 130.0f; p.wet = 26.0f;
            }
            else if (airRatio > 0.18f)
            {
                p.id = AudioAnalyzerEngine::ProfileId::VocalWarmStudioHall;
                rec.profileName = "Vocals (Warm Studio Hall - Sheen)";
                p.predelayMs   = 30.0f; p.size = 68.0f; p.decaySeconds = 2.10f; p.diffusion = 90.0f; p.damping = 28.0f;
                p.lowCut = 125.0f; p.highCut = 16000.0f; p.ducking = 35.0f; p.shimmer = 6.0f; p.width = 140.0f; p.wet = 30.0f;
            }
            else
            {
                p.id = AudioAnalyzerEngine::ProfileId::VocalsLead;
                rec.profileName = "Vocals (Studio Lead - Auto Ducking)";
                p.predelayMs   = 28.0f; p.size = 62.0f; p.decaySeconds = 1.85f; p.diffusion = 88.0f; p.damping = 35.0f;
                p.lowCut = 130.0f; p.highCut = 12500.0f; p.ducking = 35.0f; p.shimmer = 0.0f; p.width = 135.0f; p.wet = 28.0f;
            }
        }
        else if (bestId == AudioAnalyzerEngine::ProfileId::DrumsPunchyRoom)
        {
            rec.instrumentCategory = "DRUMS";
            rec.profileName = "Drums (Punchy Studio Room)";
            p.predelayMs   = 8.0f; p.size = 32.0f; p.decaySeconds = 0.80f; p.diffusion = 85.0f; p.damping = 55.0f;
            p.lowCut = 180.0f; p.highCut = 9500.0f; p.ducking = 0.0f; p.shimmer = 0.0f; p.width = 105.0f; p.wet = 16.0f;
        }
        else if (bestId == AudioAnalyzerEngine::ProfileId::GuitarKeysWarmChamber)
        {
            rec.instrumentCategory = "GUITAR";
            rec.profileName = "Guitar (Warm Acoustic Chamber)";
            p.predelayMs   = 18.0f; p.size = 50.0f; p.decaySeconds = 1.25f; p.diffusion = 75.0f; p.damping = 48.0f;
            p.lowCut = 140.0f; p.highCut = 11000.0f; p.ducking = 10.0f; p.shimmer = 0.0f; p.width = 120.0f; p.wet = 15.0f;
        }
        else if (bestId == AudioAnalyzerEngine::ProfileId::PianoKeysConcertHall)
        {
            rec.instrumentCategory = "PIANO / KEYS";
            rec.profileName = "Piano & Keys (Concert Grand Hall)";
            p.predelayMs   = 24.0f; p.size = 76.0f; p.decaySeconds = 2.30f; p.diffusion = 88.0f; p.damping = 30.0f;
            p.lowCut = 85.0f; p.highCut = 16000.0f; p.ducking = 10.0f; p.shimmer = 0.0f; p.width = 140.0f; p.wet = 28.0f;
        }
        else if (bestId == AudioAnalyzerEngine::ProfileId::BassCleanTightRoom)
        {
            rec.instrumentCategory = "BASS";
            rec.profileName = "Bass (Tight Acoustic Dimension)";
            p.predelayMs   = 5.0f; p.size = 26.0f; p.decaySeconds = 0.40f; p.diffusion = 65.0f; p.damping = 70.0f;
            p.lowCut = 240.0f; p.highCut = 6500.0f; p.ducking = 30.0f; p.shimmer = 0.0f; p.width = 85.0f; p.wet = 14.0f;
        }
        else
        {
            rec.instrumentCategory = "SYNTH / PAD";
            rec.profileName = "Synth & Pad (Ethereal Cosmic Bloom)";
            p.predelayMs   = 45.0f; p.size = 88.0f; p.decaySeconds = 4.20f; p.diffusion = 95.0f; p.damping = 22.0f;
            p.lowCut = 80.0f; p.highCut = 18000.0f; p.ducking = 10.0f; p.shimmer = 18.0f; p.width = 175.0f; p.wet = 36.0f;
        }

        rec.confidence = juce::jlimit (0.0f, 1.0f, maxScore / 100.0f);
        clampToAPVTS (p);
        return rec;
    }
};
