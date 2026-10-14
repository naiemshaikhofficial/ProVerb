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

    return layout;
}
