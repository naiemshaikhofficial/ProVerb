#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Licensing/LicenseClient.h"

namespace ProducerToy::UI
{
    class MinimalistButtonLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                   const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override
        {
            auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
            const float cornerR = 7.0f;

            juce::Colour bg = backgroundColour;
            if (shouldDrawButtonAsDown)
                bg = bg.darker (0.15f);
            else if (shouldDrawButtonAsHighlighted)
                bg = bg.brighter (0.08f);

            g.setColour (bg);
            g.fillRoundedRectangle (bounds, cornerR);

            if (backgroundColour.getBrightness() < 0.5f)
            {
                g.setColour (shouldDrawButtonAsHighlighted ? juce::Colour (0xff3f3f46) : juce::Colour (0xff27272a));
                g.drawRoundedRectangle (bounds, cornerR, 1.0f);
            }
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                             bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) override
        {
            auto bounds = button.getLocalBounds();
            g.setFont (juce::FontOptions ("Segoe UI", 12.0f, juce::Font::bold));
            g.setColour (button.findColour (juce::TextButton::textColourOffId));
            g.drawText (button.getButtonText(), bounds, juce::Justification::centred, false);
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

        bool isPairingActive { false };
        juce::String pairingCode;
        juce::String statusMessage;
        bool isError { false };
        int spinnerTick { 0 };

        juce::TextButton closeButton;
        juce::TextButton continueButton { "Continue to Plugin" };
        juce::TextButton activateButton { "Activate via Producer Toy" };
        juce::TextButton buyButton { "Buy ProVerb ($9.99)" };
        juce::TextButton manageSeatsButton { "Manage Account Seats" };
        juce::TextButton deactivateButton { "Deactivate This Machine" };
        juce::TextButton supportButton { "Licensing Support" };
        juce::TextButton cancelPairingButton { "Cancel" };

        void startActivation();
        void handleDeactivate();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicensePanel)
    };
}
