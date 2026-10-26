#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ProVerbIDs
{
    static const juce::ParameterID inputGain     { "inputGain", 1 };
    static const juce::ParameterID predelayMs    { "predelayMs", 1 };
    static const juce::ParameterID decaySeconds  { "decaySeconds", 1 };
    static const juce::ParameterID size          { "size", 1 };
    static const juce::ParameterID diffusion     { "diffusion", 1 };
    static const juce::ParameterID damping       { "damping", 1 };
    static const juce::ParameterID width         { "width", 1 };
    static const juce::ParameterID wet           { "wet", 1 };
    static const juce::ParameterID dry           { "dry", 1 };
    static const juce::ParameterID outputGain    { "outputGain", 1 };
    static const juce::ParameterID ducking       { "ducking", 1 };
    static const juce::ParameterID shimmer       { "shimmer", 1 };
    static const juce::ParameterID lowCut        { "lowCut", 1 };
    static const juce::ParameterID highCut       { "highCut", 1 };
    static const juce::ParameterID freeze        { "freeze", 1 };
    static const juce::ParameterID bypass        { "bypass", 1 };
    static const juce::ParameterID midSide       { "midSide", 1 };

    // Smart AI Automation Macros & Sidechain Routing
    static const juce::ParameterID smartSpace        { "smartSpace", 1 };
    static const juce::ParameterID smartClarity      { "smartClarity", 1 };
    static const juce::ParameterID smartPumping      { "smartPumping", 1 };
    static const juce::ParameterID duckingSource     { "duckingSource", 1 };
    static const juce::ParameterID duckingThreshold  { "duckingThreshold", 1 };
    static const juce::ParameterID duckingAttack     { "duckingAttack", 1 };
    static const juce::ParameterID duckingRelease    { "duckingRelease", 1 };

    // Interactive 5-Band Reverb EQ Parameters
    static const juce::ParameterID eqLowShelfFreq    { "eqLowShelfFreq", 1 };
    static const juce::ParameterID eqLowShelfGain    { "eqLowShelfGain", 1 };
    static const juce::ParameterID eqMidFreq         { "eqMidFreq", 1 };
    static const juce::ParameterID eqMidGain         { "eqMidGain", 1 };
    static const juce::ParameterID eqMidQ            { "eqMidQ", 1 };
    static const juce::ParameterID eqHighShelfFreq   { "eqHighShelfFreq", 1 };
    static const juce::ParameterID eqHighShelfGain   { "eqHighShelfGain", 1 };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createProVerbParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Input Gain (-24dB to +12dB)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::inputGain, "Input Gain",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // Pre-Delay (0 to 300 ms)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::predelayMs, "Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 300.0f, 1.0f), 20.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    // Decay Time (0.1 to 30.0 s)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::decaySeconds, "Decay Time",
        juce::NormalisableRange<float> (0.1f, 30.0f, 0.05f, 0.4f), 2.8f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    // Room Size (10% to 100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::size, "Space Size",
        juce::NormalisableRange<float> (10.0f, 100.0f, 0.5f), 70.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Diffusion (0% to 100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::diffusion, "Diffusion",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f), 75.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // High Damping (0% to 100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::damping, "Damping",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f), 45.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Stereo Width (0% Mono to 200% Extra Wide)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::width, "Stereo Width",
        juce::NormalisableRange<float> (0.0f, 200.0f, 1.0f), 120.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Smart Vocal Ducking (0% to 100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::ducking, "Ducking",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Shimmer Pitch Feedback (0% to 100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::shimmer, "Shimmer",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Low Cut Filter (20Hz to 1000Hz)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::lowCut, "Low Cut",
        juce::NormalisableRange<float> (20.0f, 1000.0f, 1.0f, 0.35f), 80.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    // High Cut Filter (1000Hz to 20000Hz)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::highCut, "High Cut",
        juce::NormalisableRange<float> (1000.0f, 20000.0f, 10.0f, 0.35f), 14000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    // Wet Signal Level (0% to 100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::wet, "Wet Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f), 30.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Dry Signal Level (0% to 100%)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::dry, "Dry Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Output Gain (-24dB to +12dB)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::outputGain, "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // Freeze Mode (Infinite Drone)
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ProVerbIDs::freeze, "Freeze", false));

    // Master Bypass
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ProVerbIDs::bypass, "Bypass", false));

    // Mid/Side Routing Focus Mode
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ProVerbIDs::midSide, "Mid/Side Focus", false));

    // =========================================================================
    // Next-Level Smart AI Automation Macros & Professional Sidechain Parameters
    // =========================================================================

    // AI Space Morph Macro (0% to 100%, Default 50% Neutral)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::smartSpace, "AI Space Morph",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // AI Anti-Mud & Clarity Macro (0% to 100%, Default 50% Neutral)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::smartClarity, "AI Clarity & Anti-Mud",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // AI Rhythm & Ducking Pump Macro (0% to 100%, Default 50% Neutral)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::smartPumping, "AI Rhythm Pump",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Ducking Trigger Source (Auto / Internal / External Sidechain)
    juce::StringArray duckingSourceChoices;
    duckingSourceChoices.add ("Auto (Smart AI)");
    duckingSourceChoices.add ("Internal (Dry)");
    duckingSourceChoices.add ("External (Sidechain)");
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ProVerbIDs::duckingSource, "Ducking Source",
        duckingSourceChoices, 0));

    // Ducking Threshold (-48 dB to -6 dB, Default -32 dB)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::duckingThreshold, "Ducking Threshold",
        juce::NormalisableRange<float> (-48.0f, -6.0f, 0.5f), -32.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // Ducking Attack Time (1 ms to 50 ms, Default 8 ms)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::duckingAttack, "Ducking Attack",
        juce::NormalisableRange<float> (1.0f, 50.0f, 0.5f), 8.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    // Ducking Release Time (20 ms to 1000 ms, Default 160 ms, logarithmic skew)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::duckingRelease, "Ducking Release",
        juce::NormalisableRange<float> (20.0f, 1000.0f, 1.0f, 0.4f), 160.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    // =========================================================================
    // Interactive 5-Band Reverb EQ Parameters
    // =========================================================================

    // Node 2: Low Shelf Frequency (40 Hz to 800 Hz, Default 250 Hz)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::eqLowShelfFreq, "EQ Low Shelf Freq",
        juce::NormalisableRange<float> (40.0f, 800.0f, 1.0f, 0.4f), 250.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    // Node 2: Low Shelf Gain (-12 dB to +12 dB, Default 0 dB)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::eqLowShelfGain, "EQ Low Shelf Gain",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // Node 3: Parametric Mid Bell Frequency (100 Hz to 8000 Hz, Default 1200 Hz)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::eqMidFreq, "EQ Mid Freq",
        juce::NormalisableRange<float> (100.0f, 8000.0f, 1.0f, 0.35f), 1200.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    // Node 3: Parametric Mid Bell Gain (-12 dB to +12 dB, Default 0 dB)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::eqMidGain, "EQ Mid Gain",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // Node 3: Parametric Mid Bell Q (0.2 to 8.0, Default 0.70)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::eqMidQ, "EQ Mid Q",
        juce::NormalisableRange<float> (0.2f, 8.0f, 0.05f, 0.5f), 0.70f));

    // Node 4: High Shelf Frequency (1500 Hz to 16000 Hz, Default 5000 Hz)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::eqHighShelfFreq, "EQ High Shelf Freq",
        juce::NormalisableRange<float> (1500.0f, 16000.0f, 10.0f, 0.4f), 5000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    // Node 4: High Shelf Gain (-12 dB to +12 dB, Default 0 dB)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ProVerbIDs::eqHighShelfGain, "EQ High Shelf Gain",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}
