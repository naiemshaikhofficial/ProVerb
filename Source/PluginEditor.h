#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/ProVerbLookAndFeel.h"
#include "UI/SpaceVisualizer.h"
#include "UI/ModernReverbEQ.h"
#include "UI/SmartAutoLearnButton.h"
#include "UI/LicensePanel.h"
#include "Licensing/LicenseClient.h"

class ProVerbAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit ProVerbAudioProcessorEditor (ProVerbAudioProcessor&);
    ~ProVerbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    ProVerbAudioProcessor& audioProcessor;
    ProVerbLookAndFeel customLaf;

    // Official ProducerToy Mascot, Brand & ProVerb Logos (Master & High-DPI Supersampled Caches)
    juce::Image logoImage;
    juce::Image proverbLogoImage;
    juce::Image proverbLogoCached;
    juce::Image ptLogoCached;
    juce::Rectangle<int> proverbLogoBounds;
    juce::Rectangle<int> ptLogoBounds;
    int headerDividerX = 155;

    // Module 1: 3D Holographic Cylinder & Colorful Acoustic Aura Visualizer
    SpaceVisualizer visualizer;

    // Module 6: Modern Interactive Visual EQ & Real-Time Spectrum
    ModernReverbEQ modernEQ;

    // Module 2: Filters & Routing
    juce::ToggleButton midSideButton { "MID / SIDE" };
    juce::Slider hCutSlider;
    juce::Slider lCutSlider;
    juce::Slider duckSlider;

    juce::Label hCutLabel;
    juce::Label lCutLabel;
    juce::Label duckLabel;

    // Module 3: Space & Time
    juce::Slider delaySlider;
    juce::Slider sizeSlider;
    juce::Slider diffSlider;
    juce::Slider shimmerSlider;

    juce::Label delayLabel;
    juce::Label sizeLabel;
    juce::Label diffLabel;
    juce::Label shimmerLabel;

    // Module 4: Decay & Damping
    juce::Slider decSlider;
    juce::Slider dampSlider;
    juce::ToggleButton freezeButton { "FREEZE" };

    juce::Label decLabel;
    juce::Label dampLabel;

    // Module 5: Console Mixer Faders (Linear Vertical Sliders)
    juce::Slider inGainFader;
    juce::Slider dryFader;
    juce::Slider wetFader;
    juce::Slider outGainFader;
    juce::Slider sepSlider; // ◀ SEP ▶ Stereo separation knob

    juce::Label inGainLabel;
    juce::Label dryLabel;
    juce::Label wetLabel;
    juce::Label outGainLabel;
    juce::Label sepLabel;

    // Top Bar
    juce::ComboBox presetBox;
    juce::ToggleButton bypassButton { "BYPASS" };
    SmartAutoLearnButton smartAutoLearnButton;
    int analysisWatchdogTicks { 0 };
    bool wasAnalyzing { false };
    int lastMatchedPresetIndex { -2 };

    // Enhanced Top Bar Controls
    juce::TextEditor searchBox;
    juce::TextButton abButton { "A" };
    juce::TextButton copyABButton { "A->B" };
    juce::TextButton scaleButton { "100%" };
    juce::TextButton undoAutoLearnButton { "UNDO" };
    float currentScaleFactor { 1.0f };

    std::unique_ptr<ProducerToy::UI::LicensePanel> licensePanel;
    void showLicensePanel();
    void hideLicensePanel();

    struct ParameterSnapshot
    {
        std::map<juce::String, float> values;
    };
    ParameterSnapshot snapshotA;
    ParameterSnapshot snapshotB;
    bool isStateBActive { false };

    void captureSnapshot (ParameterSnapshot& target);
    void applySnapshot (const ParameterSnapshot& source);
    juce::File getUserPresetDirectory() const;
    void saveUserToyPreset (const juce::String& presetName);
    void loadUserToyPreset (const juce::File& file);
    void rebuildPresetMenu();
    void setGuiScale (float scale);
    void performPresetSearch (const juce::String& query);
    std::shared_ptr<juce::FileChooser> fileChooser;

    // Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> delayAttach;
    std::unique_ptr<SliderAttachment> sizeAttach;
    std::unique_ptr<SliderAttachment> decAttach;
    std::unique_ptr<SliderAttachment> diffAttach;
    std::unique_ptr<SliderAttachment> dampAttach;
    std::unique_ptr<SliderAttachment> hCutAttach;
    std::unique_ptr<SliderAttachment> lCutAttach;
    std::unique_ptr<SliderAttachment> duckAttach;
    std::unique_ptr<SliderAttachment> shimmerAttach;
    std::unique_ptr<SliderAttachment> sepAttach;
    std::unique_ptr<SliderAttachment> dryAttach;
    std::unique_ptr<SliderAttachment> wetAttach;
    std::unique_ptr<SliderAttachment> inGainAttach;
    std::unique_ptr<SliderAttachment> outGainAttach;
    std::unique_ptr<ButtonAttachment> freezeAttach;
    std::unique_ptr<ButtonAttachment> bypassAttach;
    std::unique_ptr<ButtonAttachment> midSideAttach;

    struct KnobContextMenuHelper : public juce::MouseListener
    {
        double clipboardValue { 0.0 };
        bool hasClipboard { false };

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (auto* slider = dynamic_cast<juce::Slider*> (e.eventComponent))
                {
                    juce::PopupMenu menu;
                    double defaultVal = slider->getDoubleClickReturnValue();
                    juce::String suffix = slider->getTextValueSuffix();

                    menu.addItem (1, "Type Exact Value...");
                    menu.addSeparator();
                    menu.addItem (2, "Copy Value (" + juce::String (slider->getValue(), 1) + suffix + ")");
                    menu.addItem (3, "Paste Value" + (hasClipboard ? " (" + juce::String (clipboardValue, 1) + suffix + ")" : ""), hasClipboard);
                    menu.addSeparator();
                    menu.addItem (4, "Reset to Default (" + juce::String (defaultVal, 1) + suffix + ")");

                    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (slider),
                        [this, slider, defaultVal](int result)
                        {
                            if (result == 1)
                            {
                                slider->showTextBox();
                            }
                            else if (result == 2)
                            {
                                clipboardValue = slider->getValue();
                                hasClipboard = true;
                            }
                            else if (result == 3 && hasClipboard)
                            {
                                slider->setValue (clipboardValue, juce::sendNotificationSync);
                            }
                            else if (result == 4)
                            {
                                slider->setValue (defaultVal, juce::sendNotificationSync);
                            }
                        });
                }
            }
        }
    };

    KnobContextMenuHelper contextMenuHelper;

    void setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix = "", double defaultVal = 0.0);
    void setupFader (juce::Slider& slider, juce::Label& label, const juce::String& text, double defaultVal = 0.0);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProVerbAudioProcessorEditor)
};
