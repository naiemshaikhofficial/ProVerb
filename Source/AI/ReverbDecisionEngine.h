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

        // Base recipe mapping based on winning category
        switch (topClass)
        {
            case ProVerbAI::ClassVocals:
            {
                rec.instrumentCategory = "VOCALS (AI)";
                const float airRatio   = snapshot.features[ProVerbAI::AirRatio];
                const float envDyn     = snapshot.features[ProVerbAI::EnvDynamics];
                const float bassRatio  = snapshot.features[ProVerbAI::BassRatio];
                const float coreRatio  = snapshot.features[ProVerbAI::VocalCoreRatio];
                const float transDens  = snapshot.features[ProVerbAI::TransientDensity];

                if (envDyn > 1.8f || transDens > 2.0f)
                {
                    // Fast Rap / Trap Vocal: tight space, heavy ducking for clear lyric articulation
                    p.id = AudioAnalyzerEngine::ProfileId::VocalsRap;
                    rec.profileName = "Vocals (Rap & Trap - Tight Space)";
                    p.predelayMs   = 14.0f;
                    p.size         = 44.0f;
                    p.decaySeconds = 0.95f;
                    p.diffusion    = 84.0f;
                    p.damping      = 42.0f;
                    p.lowCut       = 160.0f;
                    p.highCut      = 13000.0f;
                    p.ducking      = 48.0f;
                    p.shimmer      = 0.0f;
                    p.width        = 125.0f;
                    p.wet          = 16.0f;
                }
                else if (airRatio > 0.16f && bassRatio < 0.20f)
                {
                    // Female Vocal: fundamental in 180-320Hz, high silky air sheen
                    p.id = AudioAnalyzerEngine::ProfileId::VocalsFemaleAiry;
                    rec.profileName = "Female Vocal (Silky Air Plate - AI)";
                    p.predelayMs   = 32.0f;
                    p.size         = 68.0f;
                    p.decaySeconds = 2.10f;
                    p.diffusion    = 90.0f;
                    p.damping      = 26.0f;
                    p.lowCut       = 190.0f;   // Cuts female chest boominess
                    p.highCut      = 15500.0f; // Silky high air
                    p.ducking      = 38.0f;
                    p.shimmer      = 5.0f;
                    p.width        = 140.0f;
                    p.wet          = 20.0f;
                }
                else
                {
                    // Male Vocal: fundamental in 85-180Hz, warm chest resonance
                    p.id = AudioAnalyzerEngine::ProfileId::VocalsMaleWarm;
                    rec.profileName = "Male Vocal (Abbey Road Warm Room - AI)";
                    p.predelayMs   = 26.0f;
                    p.size         = 62.0f;
                    p.decaySeconds = (coreRatio > 0.35f ? 1.70f : 1.80f);
                    p.diffusion    = 88.0f;
                    p.damping      = 34.0f;
                    p.lowCut       = 130.0f;   // Abbey Road filter trick
                    p.highCut      = 12000.0f;
                    p.ducking      = 36.0f;
                    p.shimmer      = 0.0f;
                    p.width        = 130.0f;
                    p.wet          = 18.0f;
                }
                break;
            }

            case ProVerbAI::ClassDrums:
            {
                rec.instrumentCategory = "DRUMS (AI)";
                const float subRatio  = snapshot.features[ProVerbAI::SubRatio];
                const float bassRatio = snapshot.features[ProVerbAI::BassRatio];
                const float coreRatio = snapshot.features[ProVerbAI::VocalCoreRatio];
                const float crest     = snapshot.features[ProVerbAI::CrestFactor];

                if (subRatio > 0.35f)
                {
                    // Kick Drum / Heavy Sub: tight dry space, protect low-end
                    p.id = AudioAnalyzerEngine::ProfileId::DrumsKickLowClean;
                    rec.profileName = "Kick & Sub (Tight Low-End Clean - AI)";
                    p.predelayMs   = 6.0f;
                    p.size         = 25.0f;
                    p.decaySeconds = 0.50f;
                    p.diffusion    = 80.0f;
                    p.damping      = 60.0f;
                    p.lowCut       = 240.0f;
                    p.highCut      = 8000.0f;
                    p.ducking      = 0.0f;
                    p.shimmer      = 0.0f;
                    p.width        = 90.0f;
                    p.wet          = 10.0f;
                }
                else if (bassRatio > 0.28f && coreRatio > 0.20f && crest < 4.5f)
                {
                    // Tabla / Dholak / Hand Percussion
                    p.id = AudioAnalyzerEngine::ProfileId::PercussionTablaHand;
                    rec.profileName = "Tabla & Hand Percussion (Intimate Studio - AI)";
                    p.predelayMs   = 12.0f;
                    p.size         = 44.0f;
                    p.decaySeconds = 1.05f;
                    p.diffusion    = 86.0f;
                    p.damping      = 42.0f;
                    p.lowCut       = 130.0f;
                    p.highCut      = 11000.0f;
                    p.ducking      = 10.0f;
                    p.shimmer      = 0.0f;
                    p.width        = 115.0f;
                    p.wet          = 18.0f;
                }
                else
                {
                    // Snare & Drum Kit
                    p.id = AudioAnalyzerEngine::ProfileId::DrumsPunchyRoom;
                    rec.profileName = "Drums & Snare (Punchy Studio Room - AI)";
                    p.predelayMs   = 10.0f;
                    p.size         = 36.0f;
                    p.decaySeconds = 0.80f;
                    p.diffusion    = 90.0f;
                    p.damping      = 52.0f;
                    p.lowCut       = 180.0f;
                    p.highCut      = 9800.0f;
                    p.ducking      = 0.0f;
                    p.shimmer      = 0.0f;
                    p.width        = 110.0f;
                    p.wet          = 17.0f;
                }
                break;
            }

            case ProVerbAI::ClassGuitar:
            {
                rec.instrumentCategory = "STRINGS / GUITAR (AI)";
                const float transDens = snapshot.features[ProVerbAI::TransientDensity];
                const float airRatio  = snapshot.features[ProVerbAI::AirRatio];
                const float presRatio = snapshot.features[ProVerbAI::VocalPresRatio];

                if (transDens >= 2.5f && (airRatio + presRatio) > 0.22f)
                {
                    // Sitar, Santoor, Sarod: rapid plucking, sympathetic string shimmer
                    p.id = AudioAnalyzerEngine::ProfileId::StringsPluckedSitar;
                    rec.profileName = "Sitar & Santoor (Resonant Shimmer Space - AI)";
                    p.predelayMs   = 22.0f;
                    p.size         = 64.0f;
                    p.decaySeconds = 2.10f;
                    p.diffusion    = 88.0f;
                    p.damping      = 32.0f;
                    p.lowCut       = 160.0f;
                    p.highCut      = 13500.0f;
                    p.ducking      = 18.0f;
                    p.shimmer      = 10.0f;
                    p.width        = 135.0f;
                    p.wet          = 18.0f;
                }
                else
                {
                    // Acoustic / Electric Guitar: warm woody chamber
                    p.id = AudioAnalyzerEngine::ProfileId::GuitarKeysWarmChamber;
                    rec.profileName = "Acoustic Guitar (Warm Woody Chamber - AI)";
                    p.predelayMs   = 18.0f;
                    p.size         = 52.0f;
                    p.decaySeconds = 1.30f;
                    p.diffusion    = 80.0f;
                    p.damping      = 45.0f;
                    p.lowCut       = 140.0f;
                    p.highCut      = 11500.0f;
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
                p.predelayMs   = 24.0f;
                p.size         = 74.0f;
                p.decaySeconds = 2.20f;
                p.diffusion    = 88.0f;
                p.damping      = 32.0f;
                p.lowCut       = 95.0f;
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
                rec.profileName = "Bass (Tight Dimension - Clean Sub - AI)";
                p.predelayMs   = 4.0f;
                p.size         = 24.0f;
                p.decaySeconds = 0.40f;
                p.diffusion    = 65.0f;
                p.damping      = 70.0f;
                p.lowCut       = 240.0f;
                p.highCut      = 6000.0f;
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
                p.predelayMs   = 40.0f;
                p.size         = 88.0f;
                p.decaySeconds = 3.80f;
                p.diffusion    = 95.0f;
                p.damping      = 24.0f;
                p.lowCut       = 80.0f;
                p.highCut      = 17500.0f;
                p.ducking      = 10.0f;
                p.shimmer      = 16.0f;
                p.width        = 170.0f;
                p.wet          = 28.0f;
                break;
            }

            default: // Other / Woodwinds / Mixed
            {
                const float coreRatio = snapshot.features[ProVerbAI::VocalCoreRatio];
                const float crest     = snapshot.features[ProVerbAI::CrestFactor];
                const float transDens = snapshot.features[ProVerbAI::TransientDensity];
                const float airRatio  = snapshot.features[ProVerbAI::AirRatio];
                const float subRatio  = snapshot.features[ProVerbAI::SubRatio];

                if (coreRatio > 0.32f && crest < 2.8f && transDens < 1.3f && airRatio > 0.12f && subRatio < 0.08f)
                {
                    // Woodwinds & Flute (Bansuri, Shakuhachi, Flute)
                    rec.instrumentCategory = "WOODWINDS / FLUTE (AI)";
                    p.id = AudioAnalyzerEngine::ProfileId::WoodwindsFlute;
                    rec.profileName = "Woodwind & Flute (Ethereal Breath Hall - AI)";
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
