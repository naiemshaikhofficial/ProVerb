#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <algorithm>

/**
 * AudioFeatureExtractor
 * 
 * Defines the exact feature extraction contract for ProVerb's AI Inference Engine.
 * 
 * MODEL INPUT CONTRACT:
 * - Tensor Shape: [1, 9] (Batch size 1, 9 acoustic features)
 * - Data Type: Float32
 * - Feature ordering:
 *     [0] subRatio        - Energy ratio in sub band (<95 Hz)
 *     [1] bassRatio       - Energy ratio in bass / warmth band (95 Hz - 320 Hz)
 *     [2] vocalCoreRatio  - Energy ratio in vocal core / body band (320 Hz - 1800 Hz)
 *     [3] vocalPresRatio  - Energy ratio in vocal presence band (1800 Hz - 6500 Hz)
 *     [4] airRatio        - Energy ratio in air / treble band (>6500 Hz)
 *     [5] crestFactor     - Peak to RMS ratio (transient dynamic spike indicator)
 *     [6] zcr             - Zero-crossing rate per sample
 *     [7] transientDensity- Detected onsets / transient spikes per second
 *     [8] envDynamics     - Envelope variance scaled (syllabic / rhythmic movement)
 */
namespace ProVerbAI
{
    static constexpr std::size_t numFeatures = 9;
    static constexpr std::size_t numClasses = 7;

    enum FeatureIndex : int
    {
        SubRatio = 0,
        BassRatio = 1,
        VocalCoreRatio = 2,
        VocalPresRatio = 3,
        AirRatio = 4,
        CrestFactor = 5,
        Zcr = 6,
        TransientDensity = 7,
        EnvDynamics = 8
    };

    enum ClassIndex : int
    {
        ClassDrums = 0,
        ClassVocals = 1,
        ClassGuitar = 2,
        ClassPiano = 3,
        ClassBass = 4,
        ClassSynthPad = 5,
        ClassOtherUncertain = 6
    };

    struct FeatureSnapshot
    {
        std::array<float, numFeatures> features {};
        float peakLevel { 0.0f };
        double meanRms { 0.0 };
        int64_t samplesAccumulated { 0 };
        double sampleRate { 44100.0 };
        bool isSilence { false };

        void sanitize() noexcept
        {
            for (auto& f : features)
            {
                if (!std::isfinite (f))
                    f = 0.0f;
            }

            // Clamp spectral energy ratios to [0.0, 1.0]
            features[SubRatio]       = juce::jlimit (0.0f, 1.0f, features[SubRatio]);
            features[BassRatio]      = juce::jlimit (0.0f, 1.0f, features[BassRatio]);
            features[VocalCoreRatio] = juce::jlimit (0.0f, 1.0f, features[VocalCoreRatio]);
            features[VocalPresRatio] = juce::jlimit (0.0f, 1.0f, features[VocalPresRatio]);
            features[AirRatio]       = juce::jlimit (0.0f, 1.0f, features[AirRatio]);

            // Crest factor strictly >= 1.0
            features[CrestFactor]    = juce::jlimit (1.0f, 50.0f, features[CrestFactor]);

            // Zero Crossing Rate in [0.0, 1.0]
            features[Zcr]            = juce::jlimit (0.0f, 1.0f, features[Zcr]);

            // Transient density bounded
            features[TransientDensity] = juce::jlimit (0.0f, 30.0f, features[TransientDensity]);

            // Envelope dynamics non-negative
            features[EnvDynamics]    = juce::jlimit (0.0f, 100.0f, features[EnvDynamics]);

            if (!std::isfinite (peakLevel)) peakLevel = 0.0f;
            if (!std::isfinite (meanRms))   meanRms = 0.0;
        }
    };
}
