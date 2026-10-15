#include "LicensePanel.h"
#include <BinaryData.h>

namespace ProducerToy::UI
{
    namespace
    {
        juce::Font getPanelFont(float size, int style = juce::Font::plain)
        {
            return juce::Font(juce::FontOptions("Segoe UI", size, style));
        }
    }

    LicensePanel::LicensePanel(std::function<void()> onCloseCallback)
        : onClose(std::move(onCloseCallback))
    {
        setOpaque(false);
        proverbLogoImage = juce::ImageFileFormat::loadFrom(BinaryData::proverb_logo_png,
                                                            BinaryData::proverb_logo_pngSize);

        addAndMakeVisible(closeButton);
        closeButton.setLookAndFeel(&buttonLnF);
        closeButton.setButtonText(juce::CharPointer_UTF8("\xc3\x97"));
        closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0x00000000));
        closeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff71717a));
        closeButton.onClick = [this]
        {
            if (state.isActivated && onClose)
                onClose();
        };

        addAndMakeVisible(continueButton);
        continueButton.setLookAndFeel(&buttonLnF);
        continueButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffffff));
        continueButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff09090b));
        continueButton.onClick = [this]
        {
            if (state.isActivated && onClose)
                onClose();
        };

        addAndMakeVisible(activateButton);
        activateButton.setLookAndFeel(&buttonLnF);
        activateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffffff));
        activateButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff09090b));
        activateButton.onClick = [this] { startActivation(); };

        addAndMakeVisible(copyDeviceIdButton);
        copyDeviceIdButton.setLookAndFeel(&buttonLnF);
        copyDeviceIdButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
        copyDeviceIdButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
        copyDeviceIdButton.onClick = [this]
        {
            juce::SystemClipboard::copyTextToClipboard(Licensing::LicenseClient::getInstance().getHardwareDeviceHash());
            statusMessage = "Device ID copied. Send it with proof of purchase to request a license file.";
            isError = false;
            repaint();
        };

        addAndMakeVisible(buyButton);
        buyButton.setLookAndFeel(&buttonLnF);
        buyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
        buyButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
        buyButton.onClick = []
        {
            juce::URL(Licensing::ProductConfig::buyUrl).launchInDefaultBrowser();
        };

        addAndMakeVisible(deactivateButton);
        deactivateButton.setLookAndFeel(&buttonLnF);
        deactivateButton.setButtonText("Remove Local License");
        deactivateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff181416));
        deactivateButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffff4053));
        deactivateButton.onClick = [this] { handleDeactivate(); };

        addAndMakeVisible(supportButton);
        supportButton.setLookAndFeel(&buttonLnF);
        supportButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff141416));
        supportButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffa1a1aa));
        supportButton.onClick = []
        {
            juce::URL(Licensing::ProductConfig::supportUrl).launchInDefaultBrowser();
        };

        refreshState();
        startTimerHz(2);
    }

    LicensePanel::~LicensePanel()
    {
        stopTimer();
        licenseChooser.reset();

        closeButton.setLookAndFeel(nullptr);
        continueButton.setLookAndFeel(nullptr);
        activateButton.setLookAndFeel(nullptr);
        copyDeviceIdButton.setLookAndFeel(nullptr);
        buyButton.setLookAndFeel(nullptr);
        deactivateButton.setLookAndFeel(nullptr);
        supportButton.setLookAndFeel(nullptr);
    }

    juce::Rectangle<int> LicensePanel::getCardBounds() const
    {
        return getLocalBounds().withSizeKeepingCentre(580, 370);
    }

    void LicensePanel::mouseDown(const juce::MouseEvent& event)
    {
        if (state.isActivated && !getCardBounds().contains(event.getPosition()) && onClose)
            onClose();
    }

    void LicensePanel::timerCallback()
    {
        if (++timerTicks % 4 != 0 || state.isActivated)
            return;

        const auto file = Licensing::LicenseClient::getLicenseFile();
        if (!file.existsAsFile())
            return;

        Licensing::LicenseClient::getInstance().initialize();
        auto newState = Licensing::LicenseClient::getInstance().getCurrentState();
        if (!newState.isActivated)
            return;

        state = newState;
        statusMessage = "Offline license loaded. ProVerb is unlocked.";
        isError = false;
        resized();
        repaint();

        juce::Component::SafePointer<LicensePanel> safeThis(this);
        juce::Timer::callAfterDelay(600, [safeThis]
        {
            if (auto* panel = safeThis.getComponent())
                if (panel->state.isActivated && panel->onClose)
                    panel->onClose();
        });
    }

    void LicensePanel::refreshState()
    {
        state = Licensing::LicenseClient::getInstance().getCurrentState();
        resized();
        repaint();
    }

    void LicensePanel::startActivation()
    {
        auto& client = Licensing::LicenseClient::getInstance();
        client.initialize();
        if (client.isLicenseValid())
        {
            refreshState();
            statusMessage = "This machine already has a valid offline license.";
            isError = false;
            repaint();
            if (onClose)
                onClose();
            return;
        }

        statusMessage = "Choose the signed ProVerb .ptlic file you received.";
        isError = false;
        repaint();

        licenseChooser = std::make_unique<juce::FileChooser>(
            "Choose your ProVerb offline license (.ptlic)", juce::File(), "*.ptlic");
        juce::Component::SafePointer<LicensePanel> safeThis(this);
        licenseChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safeThis](const juce::FileChooser& chooser)
            {
                if (auto* panel = safeThis.getComponent())
                {
                    const auto selectedFile = chooser.getResult();
                    panel->licenseChooser.reset();
                    if (!selectedFile.existsAsFile())
                        return;

                    juce::String error;
                    if (!Licensing::LicenseClient::getInstance().installLicenseFile(selectedFile, error))
                    {
                        panel->statusMessage = error;
                        panel->isError = true;
                        panel->repaint();
                        return;
                    }

                    panel->refreshState();
                    panel->statusMessage = "License verified and saved on this machine.";
                    panel->isError = false;
                    panel->repaint();

                    juce::Timer::callAfterDelay(600, [safeThis]
                    {
                        if (auto* current = safeThis.getComponent())
                            if (current->state.isActivated && current->onClose)
                                current->onClose();
                    });
                }
            });
    }

    void LicensePanel::handleDeactivate()
    {
        Licensing::LicenseClient::getInstance().deactivate();
        refreshState();
        statusMessage = "Local license file removed. No server request was made.";
        isError = false;
        repaint();

        // The editor callback refreshes the processor's cached audio-thread license state.
        // It leaves this panel open because the plugin is now locked.
        if (onClose)
            onClose();
    }

    void LicensePanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour(0xee08080a));
        auto card = getCardBounds().toFloat();

        g.setColour(juce::Colour(0xff121214));
        g.fillRoundedRectangle(card, 12.0f);
        g.setColour(juce::Colour(0xff242428));
        g.drawRoundedRectangle(card, 12.0f, 1.0f);

        if (proverbLogoImage.isValid())
        {
            const float logoWidth = 140.0f;
            const float logoHeight = logoWidth * static_cast<float>(proverbLogoImage.getHeight())
                                                   / static_cast<float>(proverbLogoImage.getWidth());
            const float logoX = card.getCentreX() - logoWidth * 0.5f;
            const float logoY = card.getY() + 24.0f;
            g.drawImage(proverbLogoImage, static_cast<int>(logoX), static_cast<int>(logoY),
                        static_cast<int>(logoWidth), static_cast<int>(logoHeight),
                        0, 0, proverbLogoImage.getWidth(), proverbLogoImage.getHeight());
            g.setFont(getPanelFont(11.5f, juce::Font::bold));
            g.setColour(juce::Colours::white);
            g.drawText("OFFLINE LICENSE", card.getX(), logoY + logoHeight + 6.0f,
                       card.getWidth(), 16.0f, juce::Justification::centred);
        }

        const float contentX = card.getX() + 38.0f;
        const float contentW = card.getWidth() - 76.0f;
        float y = card.getY() + 116.0f;

        if (state.isActivated)
        {
            g.setFont(getPanelFont(14.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xff10b981));
            g.drawText("LICENSED & ACTIVE", contentX, y, contentW, 20.0f, juce::Justification::centredLeft);
            y += 26.0f;

            g.setFont(getPanelFont(15.0f, juce::Font::bold));
            g.setColour(juce::Colours::white);
            g.drawText("Licensed to: " + state.registeredName, contentX, y, contentW, 22.0f,
                       juce::Justification::centredLeft);
            y += 22.0f;

            g.setFont(getPanelFont(12.0f));
            g.setColour(juce::Colour(0xffa1a1aa));
            g.drawText("Account: " + state.maskedEmail + "   |   Device-bound offline license",
                       contentX, y, contentW, 18.0f, juce::Justification::centredLeft);
        }
        else
        {
            g.setFont(getPanelFont(16.5f, juce::Font::bold));
            g.setColour(juce::Colour(0xffff4053));
            g.drawText("ProVerb Reverb Engine is Locked", contentX, y, contentW, 22.0f,
                       juce::Justification::centredLeft);
            y += 26.0f;

            g.setFont(getPanelFont(12.5f));
            g.setColour(juce::Colour(0xffe4e4e7));
            g.drawText("Import the signed .ptlic file supplied with your purchase.", contentX, y,
                       contentW, 18.0f, juce::Justification::centredLeft);
            y += 20.0f;

            g.setFont(getPanelFont(11.5f));
            g.setColour(juce::Colour(0xffa1a1aa));
            g.drawText("For a new machine, copy its Device ID and request a device-bound license.",
                       contentX, y, contentW, 18.0f, juce::Justification::centredLeft);
            y += 23.0f;

            const auto deviceId = Licensing::LicenseClient::getInstance().getHardwareDeviceHash();
            const auto abbreviatedId = deviceId.substring(0, juce::jmin(27, deviceId.length())) + "...";
            g.setFont(getPanelFont(10.5f));
            g.setColour(juce::Colour(0xff71717a));
            g.drawText("Device ID: " + abbreviatedId, contentX, y, contentW, 16.0f,
                       juce::Justification::centredLeft);

            if (statusMessage.isNotEmpty())
            {
                g.setFont(getPanelFont(10.5f));
                g.setColour(isError ? juce::Colour(0xffff6677) : juce::Colour(0xffa1a1aa));
                g.drawFittedText(statusMessage, juce::Rectangle<int>(
                    static_cast<int>(contentX), static_cast<int>(card.getY() + 256.0f),
                    static_cast<int>(contentW), 20), juce::Justification::centredLeft, 1);
            }
        }

        const float dividerY = card.getBottom() - 86.0f;
        g.setColour(juce::Colour(0xff222226));
        g.drawHorizontalLine(static_cast<int>(dividerY), contentX, card.getRight() - 38.0f);

        g.setFont(getPanelFont(10.0f));
        g.setColour(juce::Colour(0xff71717a));
        g.drawText("Ed25519 signed license   |   No activation network calls   |   Device-bound",
                   contentX, dividerY + 6.0f, contentW, 16.0f, juce::Justification::centredLeft);
    }

    void LicensePanel::resized()
    {
        const auto card = getCardBounds();
        const int contentX = card.getX() + 38;
        const int contentW = card.getWidth() - 76;
        const int buttonY = card.getBottom() - 52;
        auto buttonArea = juce::Rectangle<int>(contentX, buttonY, contentW, 36);

        closeButton.setBounds(card.getRight() - 34, card.getY() + 10, 24, 24);

        if (state.isActivated)
        {
            closeButton.setVisible(true);
            continueButton.setVisible(true);
            deactivateButton.setVisible(true);
            supportButton.setVisible(true);

            activateButton.setVisible(false);
            copyDeviceIdButton.setVisible(false);
            buyButton.setVisible(false);

            continueButton.setBounds(buttonArea.removeFromLeft(145));
            buttonArea.removeFromLeft(12);
            deactivateButton.setBounds(buttonArea.removeFromLeft(180));
            buttonArea.removeFromLeft(12);
            supportButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth()));
        }
        else
        {
            closeButton.setVisible(false);
            continueButton.setVisible(false);
            deactivateButton.setVisible(false);

            activateButton.setVisible(true);
            copyDeviceIdButton.setVisible(true);
            buyButton.setVisible(true);
            supportButton.setVisible(true);

            copyDeviceIdButton.setBounds(contentX, card.getY() + 224, 142, 28);

            activateButton.setBounds(buttonArea.removeFromLeft(185));
            buttonArea.removeFromLeft(10);
            buyButton.setBounds(buttonArea.removeFromLeft(145));
            buttonArea.removeFromLeft(10);
            supportButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth()));
        }
    }
}
