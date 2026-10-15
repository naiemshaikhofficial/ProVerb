#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Licensing/LicenseClient.h"

namespace ProducerToy::UI
{
    class MinimalistButtonLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                  const juce::Colour& backgroundColour,
                                  bool shouldDrawButtonAsHighlighted,
                                  bool shouldDrawButtonAsDown) override
        {
            auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
            juce::Colour bg = backgroundColour;
            if (shouldDrawButtonAsDown)
                bg = bg.darker(0.15f);
            else if (shouldDrawButtonAsHighlighted)
                bg = bg.brighter(0.08f);

            g.setColour(bg);
            g.fillRoundedRectangle(bounds, 7.0f);
            if (backgroundColour.getBrightness() < 0.5f)
            {
                g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xff3f3f46)
                                                          : juce::Colour(0xff27272a));
                g.drawRoundedRectangle(bounds, 7.0f, 1.0f);
            }
        }

        void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                            bool, bool) override
        {
            g.setFont(juce::FontOptions("Segoe UI", 12.0f, juce::Font::bold));
            g.setColour(button.findColour(juce::TextButton::textColourOffId));
            g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
        }
    };

    class LicensePanel : public juce::Component,
                         private juce::Timer
    {
    public:
        explicit LicensePanel(std::function<void()> onCloseCallback);
        ~LicensePanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void refreshState();
        juce::Rectangle<int> getCardBounds() const;

    private:
        void timerCallback() override;

        std::function<void()> onClose;
        Licensing::LicenseState state;
        juce::Image proverbLogoImage;
        MinimalistButtonLookAndFeel buttonLnF;
        std::unique_ptr<juce::FileChooser> licenseChooser;
        juce::String statusMessage;
        bool isError { false };
        int timerTicks { 0 };

        juce::TextButton closeButton;
        juce::TextButton continueButton { "Continue to Plugin" };
        juce::TextButton activateButton { "Import License File" };
        juce::TextButton copyDeviceIdButton { "Copy Device ID" };
        juce::TextButton buyButton { "Buy ProVerb ($9.99)" };
        juce::TextButton deactivateButton { "Remove Local License" };
        juce::TextButton supportButton { "Licensing Support" };

        void startActivation();
        void handleDeactivate();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicensePanel)
    };
}
