#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <cmath>

class ProVerbAudioProcessor;
class ProVerbAudioProcessorEditor;

namespace ProducerToy::UI
{
    /**
     * ReverbAssistantPanel — Ultra-Premium Neoverb-style Reverb Assistant Modal Overlay
     *
     * Flow:
     * - Stage 0: "Play audio & begin"
     *     - Style slider: Continuous Realistic <-> Dramatic gradient
     *     - Size selector: XS, S, M, L, XL notch selector + circular Dry/Wet control
     *     - Tone tiles: Clean, Dark, Bright, Airy
     *     - Next button
     * - Stage 1: "Waiting for you to play audio..."
     *     - Animated minimalist musical note with clock face & soft pulsing acoustic rings
     *     - Real-time DAW audio detection or manual "Analyze Now" button
     * - Stage 2: "Assistant is working..."
     *     - Minimalist checklist:
     *         ✂ Auto Cut: Cleaning up input signal by creating custom Pre EQ curve
     *         ☷ Unmask: Comparing wet and dry signal to create custom Post EQ curve
     *         ✦ Space Tuning: Synthesizing custom 3D acoustic reflection & tail matrix
     *     - Detected instrument badge & confidence
     *     - Customizes parameters according to Style, Size, Tone & Dry/Wet
     *     - Closes back into the main UI with red UNDO ready
     *
     * Design:
     * - Pure dark charcoal/obsidian palette (#0a0b0e, #111317, #181a22) — NO blueish tint!
     * - Violet/cyan ProVerb accent glows
     * - Clean typography and vector icons
     */
    class ReverbAssistantPanel : public juce::Component,
                                 private juce::Timer
    {
    public:
        enum class Stage
        {
            Setup,       // Play audio & begin (Style, Size, Tone, Dry/Wet)
            Listening,   // Waiting for audio playback
            Working,     // Assistant is working (Auto Cut, Unmask, Space Tuning)
            Done         // Completed, displaying result
        };

        enum class ToneOption
        {
            Clean = 0,
            Dark,
            Bright,
            Airy
        };

        ReverbAssistantPanel (ProVerbAudioProcessor& proc,
                              std::function<void()> onCloseCallback,
                              std::function<void(const juce::String&, const juce::String&)> onPresetAppliedCallback);

        ~ReverbAssistantPanel() override;

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

        void startAssistantFlow();
        void resetFlow();

    private:
        void timerCallback() override;

        void drawSetupScreen (juce::Graphics& g, juce::Rectangle<float> contentArea);
        void drawListeningScreen (juce::Graphics& g, juce::Rectangle<float> contentArea);
        void drawWorkingScreen (juce::Graphics& g, juce::Rectangle<float> contentArea);
        void drawDots (juce::Graphics& g, juce::Rectangle<float> dotArea, int activeIndex);

        void advanceToListening();
        void triggerAnalysis();
        void applyCustomizedPreset (const juce::String& instrument, const juce::String& presetName);

        juce::Rectangle<float> getCardBounds() const;
        juce::Rectangle<float> getCloseButtonBounds() const;
        juce::Rectangle<float> getNextButtonBounds() const;
        juce::Rectangle<float> getStyleSliderBounds (juce::Rectangle<float> contentArea) const;
        juce::Rectangle<float> getSizeTrackBounds (juce::Rectangle<float> contentArea) const;
        juce::Rectangle<float> getDryWetKnobBounds (juce::Rectangle<float> contentArea) const;
        juce::Rectangle<float> getToneTileBounds (juce::Rectangle<float> contentArea, int index) const;

        ProVerbAudioProcessor& processor;
        std::function<void()> onClose;
        std::function<void(const juce::String&, const juce::String&)> onPresetApplied;

        Stage currentStage { Stage::Setup };

        // Configuration State
        float styleValue { 0.5f }; // 0.0 = Realistic, 1.0 = Dramatic
        int sizeStep { 2 };        // 0=XS, 1=S, 2=M, 3=L, 4=XL
        ToneOption currentTone { ToneOption::Clean };
        float dryWetPercent { 35.0f }; // 5% to 100%

        // Interaction Tracking
        bool isDraggingStyle { false };
        bool isDraggingDryWet { false };
        float dragStartY { 0.0f };
        float dragStartDryWet { 35.0f };

        // Animation state
        float animPhase { 0.0f };
        float pulseLevel { 0.0f };
        int workingTicks { 0 };
        float simulatedProgress { 0.0f };

        // Result state
        bool analysisFinished { false };
        juce::String detectedInstrument;
        juce::String appliedPresetName;
        float confidenceScore { 0.88f };
        int autoCloseCountdown { -1 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbAssistantPanel)
    };
}
