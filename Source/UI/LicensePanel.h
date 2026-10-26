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
                bg = bg.darker(0.18f);
            else if (shouldDrawButtonAsHighlighted)
                bg = bg.brighter(0.10f);

            g.setColour(bg);
            g.fillRoundedRectangle(bounds, 6.0f);
            if (backgroundColour.getBrightness() < 0.5f)
            {
                g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xff00f0ff).withAlpha(0.5f)
                                                          : juce::Colour(0xff27272a));
                g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
            }
        }

        void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                            bool, bool) override
        {
            g.setFont(juce::FontOptions("Segoe UI", 11.5f, juce::Font::bold));
            g.setColour(button.findColour(juce::TextButton::textColourOffId));
            g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
        }
    };

    class LicensePanel : public juce::Component,
                         private juce::Timer
    {
    public:
        enum class Tab
        {
            SerialKey,
            OfflineFile,
            OnlineAccount
        };

        explicit LicensePanel(std::function<void()> onCloseCallback);
        ~LicensePanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void refreshState();
        juce::Rectangle<int> getCardBounds() const;

    private:
        void timerCallback() override;
        void setTab(Tab tab);

        std::function<void()> onClose;
        Licensing::LicenseState state;
        juce::Image proverbLogoImage;
        MinimalistButtonLookAndFeel buttonLnF;
        std::unique_ptr<juce::FileChooser> licenseChooser;

        Tab currentTab { Tab::SerialKey };
        juce::String statusMessage;
        bool isError { false };
        bool isWorking { false };
        int timerTicks { 0 };

        // Pairing session state
        juce::String activePairingCode;
        juce::String activeClaimUrl;
        int pairingCountdown { 300 };

        // Window & General controls
        juce::TextButton closeButton;
        juce::TextButton continueButton { "Done" };
        juce::TextButton buyButton { "Buy ProVerb ($9.99)" };
        juce::TextButton deactivateButton { "Deactivate" };
        juce::TextButton supportButton { "Support" };

        // Tab selection buttons
        juce::TextButton tabSerialButton { "SERIAL KEY" };
        juce::TextButton tabOfflineButton { "OFFLINE FILE" };
        juce::TextButton tabOnlineButton { "ONLINE ACCOUNT" };

        // Method 1: Serial Key Controls
        juce::TextEditor serialEditor;
        juce::TextButton activateSerialButton { "Activate Serial Key" };

        // Method 2: Offline File Controls
        juce::TextButton copyDeviceIdButton { "Copy Device ID" };
        juce::TextButton importFileButton { "Import .ptlic File" };

        // Method 3: Online Account Controls
        juce::TextButton startPairingButton { "Start Online Pairing" };
        juce::TextButton openBrowserButton { "Open Claim Page" };
        juce::TextButton cancelPairingButton { "Cancel Pairing" };

        void handleActivateSerial();
        void handleImportOffline();
        void handleStartPairing();
        void handleDeactivate();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicensePanel)
    };
}
