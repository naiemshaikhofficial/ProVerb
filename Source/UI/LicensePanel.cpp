#include "LicensePanel.h"
#include <BinaryData.h>

namespace ProducerToy::UI
{
    static juce::Font getPanelFont(float size, int style = juce::Font::plain)
    {
        return juce::Font(juce::FontOptions("Segoe UI", size, style));
    }

    LicensePanel::LicensePanel(std::function<void()> onCloseCallback)
        : onClose(std::move(onCloseCallback))
    {
        setOpaque(false);

        // Load official ProVerb vector logo
        proverbLogoImage = juce::ImageFileFormat::loadFrom (BinaryData::proverb_logo_png, BinaryData::proverb_logo_pngSize);

        // Close Button (Minimalist clean × glyph)
        addAndMakeVisible(closeButton);
        closeButton.setLookAndFeel(&buttonLnF);
        closeButton.setButtonText(juce::CharPointer_UTF8("\xc3\x97"));
        closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0x00000000));
        closeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff71717a));
        closeButton.onClick = [this]() {
            if (! state.isActivated) return;
            if (isPairingActive)
            {
                Licensing::LicenseClient::getInstance().cancelPairingFlow();
                isPairingActive = false;
            }
            if (onClose) onClose();
        };

        // Continue Button (Solid White)
        addAndMakeVisible(continueButton);
        continueButton.setLookAndFeel(&buttonLnF);
        continueButton.setButtonText("Continue to Plugin");
        continueButton.onClick = [this]() {
            if (state.isActivated && onClose) onClose();
        };
        continueButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffffff));
        continueButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff09090b));

        // Activate Button (Pure Solid White, Charcoal Dark text - Image 2)
        addAndMakeVisible(activateButton);
        activateButton.setLookAndFeel(&buttonLnF);
        activateButton.setButtonText("Activate via Producer Toy");
        activateButton.onClick = [this]() { startActivation(); };
        activateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffffff));
        activateButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff09090b));

        // Buy Button (Dark Charcoal with White text - Image 2)
        addAndMakeVisible(buyButton);
        buyButton.setLookAndFeel(&buttonLnF);
        buyButton.setButtonText("Buy ProVerb ($9.99)");
        buyButton.onClick = []() {
            juce::URL(Licensing::ProductConfig::buyUrl).launchInDefaultBrowser();
        };
        buyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
        buyButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));

        // Manage Seats Button
        addAndMakeVisible(manageSeatsButton);
        manageSeatsButton.setLookAndFeel(&buttonLnF);
        manageSeatsButton.setButtonText("Manage Account Seats");
        manageSeatsButton.onClick = []() {
            juce::URL(Licensing::ProductConfig::libraryUrl).launchInDefaultBrowser();
        };
        manageSeatsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
        manageSeatsButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));

        // Deactivate Button (Subtle Dark Charcoal with Coral-Red text)
        addAndMakeVisible(deactivateButton);
        deactivateButton.setLookAndFeel(&buttonLnF);
        deactivateButton.setButtonText("Deactivate This Machine");
        deactivateButton.onClick = [this]() { handleDeactivate(); };
        deactivateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff181416));
        deactivateButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffff4053));

        // Support Button (Dark Slate with Muted Gray text - Image 2)
        addAndMakeVisible(supportButton);
        supportButton.setLookAndFeel(&buttonLnF);
        supportButton.setButtonText("Licensing Support");
        supportButton.onClick = []() {
            juce::URL(Licensing::ProductConfig::supportUrl).launchInDefaultBrowser();
        };
        supportButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff141416));
        supportButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffa1a1aa));

        // Cancel Pairing Button
        addAndMakeVisible(cancelPairingButton);
        cancelPairingButton.setLookAndFeel(&buttonLnF);
        cancelPairingButton.setButtonText("Cancel");
        cancelPairingButton.onClick = [this]() {
            Licensing::LicenseClient::getInstance().cancelPairingFlow();
            isPairingActive = false;
            statusMessage = "Activation cancelled.";
            isError = false;
            repaint();
        };
        cancelPairingButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
        cancelPairingButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));

        refreshState();
        startTimerHz(10);
    }

    LicensePanel::~LicensePanel()
    {
        stopTimer();
        Licensing::LicenseClient::getInstance().cancelPairingFlow();

        closeButton.setLookAndFeel(nullptr);
        continueButton.setLookAndFeel(nullptr);
        activateButton.setLookAndFeel(nullptr);
        buyButton.setLookAndFeel(nullptr);
        manageSeatsButton.setLookAndFeel(nullptr);
        deactivateButton.setLookAndFeel(nullptr);
        supportButton.setLookAndFeel(nullptr);
        cancelPairingButton.setLookAndFeel(nullptr);
    }

    juce::Rectangle<int> LicensePanel::getCardBounds() const
    {
        return getLocalBounds().withSizeKeepingCentre(580, 370);
    }

    void LicensePanel::mouseDown(const juce::MouseEvent& e)
    {
        // Clicking outside the centered dialog card dismisses the modal ONLY IF activated
        if (state.isActivated && ! getCardBounds().contains(e.getPosition()))
        {
            if (onClose) onClose();
        }
    }

    void LicensePanel::timerCallback()
    {
        if (isPairingActive)
        {
            spinnerTick++;
            repaint();
        }

        // When locked, periodically check if activation succeeded (e.g. browser pairing completed or license token appeared)
        if (! state.isActivated && (spinnerTick % 10 == 0))
        {
            auto newState = Licensing::LicenseClient::getInstance().getCurrentState();
            if (newState.isActivated)
            {
                state = newState;
                isPairingActive = false;
                statusMessage = "Successfully activated! Unlocking DSP engine...";
                isError = false;
                resized();
                repaint();
                juce::Component::SafePointer<LicensePanel> safeThis (this);
                juce::Timer::callAfterDelay(1000, [safeThis]() {
                    if (auto* p = safeThis.getComponent())
                    {
                        if (p->state.isActivated && p->onClose)
                            p->onClose();
                    }
                });
            }
        }
    }

    void LicensePanel::refreshState()
    {
        state = Licensing::LicenseClient::getInstance().getCurrentState();
        repaint();
    }

    void LicensePanel::startActivation()
    {
        isPairingActive = true;
        isError = false;
        pairingCode = "...";
        statusMessage = "Connecting to Producer Toy licensing service...";
        repaint();

        juce::Component::SafePointer<LicensePanel> safeThis (this);

        Licensing::LicenseClient::getInstance().startPairingFlow(
            [safeThis](bool ok, const juce::String& code, const juce::String& err)
            {
                if (auto* p = safeThis.getComponent())
                {
                    if (ok)
                    {
                        p->pairingCode = code;
                        p->statusMessage = "Browser opened! Confirm code in your account.";
                        p->isError = false;
                    }
                    else
                    {
                        p->isPairingActive = false;
                        p->isError = true;
                        p->statusMessage = err;
                    }
                    p->repaint();
                }
            },
            [safeThis](bool ok, const juce::String& err)
            {
                if (auto* p = safeThis.getComponent())
                {
                    p->isPairingActive = false;
                    if (ok)
                    {
                        p->refreshState();
                        p->statusMessage = "Successfully activated! Full DSP unlocked.";
                        p->isError = false;
                    }
                    else
                    {
                        p->isError = true;
                        p->statusMessage = err;
                    }
                    p->repaint();
                }
            }
        );
    }

    void LicensePanel::handleDeactivate()
    {
        Licensing::LicenseClient::getInstance().deactivate();
        refreshState();
        statusMessage = "License removed from this machine. Reverb engine is locked.";
        isError = false;
        repaint();
    }

    void LicensePanel::paint(juce::Graphics& g)
    {
        // 1. Dark backdrop overlay matching website modal backdrop
        g.fillAll(juce::Colour(0xee08080a));

        auto cardBounds = getCardBounds().toFloat();

        // 2. Minimalist card surface matching Producer Toy website (#121214)
        g.setColour(juce::Colour(0xff121214));
        g.fillRoundedRectangle(cardBounds, 12.0f);

        // Thin minimalist border (#242428)
        g.setColour(juce::Colour(0xff242428));
        g.drawRoundedRectangle(cardBounds, 12.0f, 1.0f);

        // 3. Header: Official ProVerb Vector Logo & ACTIVATION Subtitle (Image 1)
        if (proverbLogoImage.isValid())
        {
            const float logoW = 140.0f;
            const float logoH = logoW * (static_cast<float>(proverbLogoImage.getHeight()) / static_cast<float>(proverbLogoImage.getWidth()));
            const float logoX = cardBounds.getCentreX() - (logoW * 0.5f);
            const float logoY = cardBounds.getY() + 24.0f;

            g.drawImage(proverbLogoImage,
                        static_cast<int>(logoX), static_cast<int>(logoY),
                        static_cast<int>(logoW), static_cast<int>(logoH),
                        0, 0, proverbLogoImage.getWidth(), proverbLogoImage.getHeight());

            g.setFont(getPanelFont(11.5f, juce::Font::bold));
            g.setColour(juce::Colour(0xffffffff));
            g.drawText("ACTIVATION", cardBounds.getX(), logoY + logoH + 6.0f, cardBounds.getWidth(), 16.0f, juce::Justification::centred);
        }

        const float contentX = cardBounds.getX() + 38.0f;
        const float contentW = cardBounds.getWidth() - 76.0f;
        float curY = cardBounds.getY() + 116.0f;

        // 4. Status Content (Image 2)
        if (state.isActivated)
        {
            g.setFont(getPanelFont(14.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xff10b981)); // Emerald green
            g.drawText("LICENSED & ACTIVE", contentX, curY, contentW, 20.0f, juce::Justification::centredLeft);

            curY += 26.0f;
            g.setFont(getPanelFont(15.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xffffffff));
            g.drawText("Licensed to: " + state.registeredName, contentX, curY, contentW, 22.0f, juce::Justification::centredLeft);

            curY += 22.0f;
            g.setFont(getPanelFont(12.0f, juce::Font::plain));
            g.setColour(juce::Colour(0xffa1a1aa));
            g.drawText("Account: " + state.maskedEmail + "   |   Single Device License", contentX, curY, contentW, 18.0f, juce::Justification::centredLeft);
        }
        else
        {
            if (isPairingActive)
            {
                g.setFont(getPanelFont(11.0f, juce::Font::bold));
                g.setColour(juce::Colour(0xffa1a1aa));
                g.drawText("DEVICE PAIRING CODE (SINGLE-USE)", contentX, curY, contentW, 16.0f, juce::Justification::centredLeft);

                curY += 22.0f;
                g.setFont(getPanelFont(26.0f, juce::Font::bold));
                g.setColour(juce::Colour(0xffffffff));
                g.drawText(pairingCode, contentX, curY, contentW, 30.0f, juce::Justification::centredLeft);

                curY += 34.0f;
                if (isError)
                {
                    // Coral-red error pill matching website auth page (#ff4053)
                    juce::Rectangle<float> errPill(contentX, curY, contentW, 28.0f);
                    g.setColour(juce::Colour(0xffff4053));
                    g.fillRoundedRectangle(errPill, 6.0f);
                    g.setFont(getPanelFont(11.0f, juce::Font::bold));
                    g.setColour(juce::Colours::black);
                    g.drawText(statusMessage, errPill, juce::Justification::centred);
                }
                else
                {
                    g.setFont(getPanelFont(12.0f, juce::Font::plain));
                    g.setColour(juce::Colour(0xffd4d4d8));
                    juce::String dots = (spinnerTick % 3 == 0) ? ".  " : ((spinnerTick % 3 == 1) ? ".. " : "...");
                    g.drawText(statusMessage + " " + dots, contentX, curY, contentW, 18.0f, juce::Justification::centredLeft);
                }
            }
            else
            {
                // Locked State matching Image 2 with auth page coral-red (#ff4053)
                g.setFont(getPanelFont(16.5f, juce::Font::bold));
                g.setColour(juce::Colour(0xffff4053)); // Website auth page coral-red
                g.drawText("ProVerb Reverb Engine is Locked", contentX, curY, contentW, 22.0f, juce::Justification::centredLeft);

                curY += 26.0f;
                // Description 1 (Image 2)
                g.setFont(getPanelFont(12.5f, juce::Font::plain));
                g.setColour(juce::Colour(0xffe4e4e7));
                g.drawText("Wet reverb audio processing is muted until this plugin is unlocked.", contentX, curY, contentW, 18.0f, juce::Justification::centredLeft);

                curY += 20.0f;
                // Description 2 (Image 2)
                g.setFont(getPanelFont(11.5f, juce::Font::plain));
                g.setColour(juce::Colour(0xff71717a));
                g.drawText("Activate with your ProducerToy.com account to unlock perpetual studio reverb.", contentX, curY, contentW, 18.0f, juce::Justification::centredLeft);
            }
        }

        // 5. Minimalist Divider Line (Image 2)
        const float lineY = cardBounds.getBottom() - 86.0f;
        g.setColour(juce::Colour(0xff222226));
        g.drawHorizontalLine(static_cast<int>(lineY), contentX, cardBounds.getRight() - 38.0f);

        // 6. Guarantee Spec Tagline (Image 2)
        g.setFont(getPanelFont(10.0f, juce::Font::plain));
        g.setColour(juce::Colour(0xff52525b));
        g.drawText("25519 Cryptographic Verification   |   Zero Audio Thread Overhead   |   Single Device Activation",
                   contentX, lineY + 6.0f, contentW, 16.0f, juce::Justification::centredLeft);
    }

    void LicensePanel::resized()
    {
        auto cardBounds = getCardBounds();
        const int contentX = cardBounds.getX() + 38;
        const int contentW = cardBounds.getWidth() - 76;

        // Close button at top right of the modal card (only visible if activated)
        closeButton.setBounds(cardBounds.getRight() - 34, cardBounds.getY() + 10, 24, 24);

        // Button row positioned at bottom of the modal card matching Image 2
        const int btnY = cardBounds.getBottom() - 52;
        auto btnArea = juce::Rectangle<int>(contentX, btnY, contentW, 36);

        if (state.isActivated)
        {
            activateButton.setVisible(false);
            buyButton.setVisible(false);
            cancelPairingButton.setVisible(false);

            closeButton.setVisible(true);
            continueButton.setVisible(true);
            manageSeatsButton.setVisible(true);
            deactivateButton.setVisible(true);
            supportButton.setVisible(true);

            continueButton.setBounds(btnArea.removeFromLeft(145));
            btnArea.removeFromLeft(10);
            manageSeatsButton.setBounds(btnArea.removeFromLeft(155));
            btnArea.removeFromLeft(10);
            deactivateButton.setBounds(btnArea.removeFromLeft(165));
            btnArea.removeFromLeft(10);
            supportButton.setBounds(btnArea.removeFromLeft(72));
        }
        else
        {
            closeButton.setVisible(false);
            continueButton.setVisible(false);
            manageSeatsButton.setVisible(false);
            deactivateButton.setVisible(false);

            if (isPairingActive)
            {
                activateButton.setVisible(false);
                buyButton.setVisible(false);
                cancelPairingButton.setVisible(true);
                supportButton.setVisible(true);

                cancelPairingButton.setBounds(btnArea.removeFromLeft(150));
                btnArea.removeFromLeft(12);
                supportButton.setBounds(btnArea.removeFromLeft(140));
            }
            else
            {
                activateButton.setVisible(true);
                buyButton.setVisible(true);
                cancelPairingButton.setVisible(false);
                supportButton.setVisible(true);

                // Exact button widths from Image 2
                activateButton.setBounds(btnArea.removeFromLeft(190));
                btnArea.removeFromLeft(12);
                buyButton.setBounds(btnArea.removeFromLeft(170));
                btnArea.removeFromLeft(12);
                supportButton.setBounds(btnArea.removeFromLeft(135));
            }
        }
    }
}
