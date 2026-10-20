#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

class ProVerbAudioProcessor;

namespace ProducerToy::UI
{
    /**
     * ReverbAssistantPanel — Ultra-Minimalist Neoverb-style Reverb Assistant
     *
     * Directly opens into the iconic minimalist listening visualizer:
     * - Musical note with integrated clock face
     * - Organic concentric acoustic pulse rings reacting dynamically to DAW playback
     * - Floating acoustic note particles
     * - Seamless automatic detection: as soon as audio plays, it learns the acoustic profile,
     *   synthesizes the tailored reverb preset, displays the detected result, and readies UNDO.
     */
    class ReverbAssistantPanel : public juce::Component,
                                 private juce::Timer
    {
    public:
        enum class State
        {
            WaitingForAudio,  // Idle, listening for DAW playback
            Analyzing,        // Audio playing, analyzing acoustic profile
            Completed         // Profile detected & preset created
        };

        ReverbAssistantPanel (ProVerbAudioProcessor& proc,
                              std::function<void()> onCloseCallback,
                              std::function<void(const juce::String&, const juce::String&)> onPresetAppliedCallback);

        ~ReverbAssistantPanel() override;

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& e) override;

        void startAssistantFlow();
        void resetFlow();

    private:
        void timerCallback() override;

        juce::Rectangle<float> getCloseButtonBounds() const;
        juce::Rectangle<float> getDoneButtonBounds() const;

        ProVerbAudioProcessor& processor;
        std::function<void()> onClose;
        std::function<void(const juce::String&, const juce::String&)> onPresetApplied;

        State currentState { State::WaitingForAudio };

        // Animation state
        float animPhase { 0.0f };
        float pulseLevel { 0.0f };
        int analysisTicks { 0 };
        int autoCloseCountdown { -1 };

        // Detection result
        bool hasFinishedAnalysis { false };
        juce::String detectedInstrument;
        juce::String appliedPresetName;
        float confidenceScore { 0.88f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbAssistantPanel)
    };
}
