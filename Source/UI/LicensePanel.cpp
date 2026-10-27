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

        // Window controls
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
        continueButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff121212));
        continueButton.onClick = [this]
        {
            if (state.isActivated && onClose)
                onClose();
        };

        addAndMakeVisible(buyButton);
        buyButton.setLookAndFeel(&buttonLnF);
        buyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00f0ff));
        buyButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff09090b));
        buyButton.onClick = []
        {
            juce::URL(Licensing::ProductConfig::buyUrl).launchInDefaultBrowser();
        };

        addAndMakeVisible(deactivateButton);
        deactivateButton.setLookAndFeel(&buttonLnF);
        deactivateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
        deactivateButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffa1a1aa));
        deactivateButton.onClick = [this] { handleDeactivate(); };

        addAndMakeVisible(supportButton);
        supportButton.setLookAndFeel(&buttonLnF);
        supportButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
        supportButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffa1a1aa));
        supportButton.onClick = []
        {
            juce::URL(Licensing::ProductConfig::supportUrl).launchInDefaultBrowser();
        };

        // Tabs
        auto setupTabBtn = [this](juce::TextButton& btn, Tab tab)
        {
            addAndMakeVisible(btn);
            btn.setLookAndFeel(&buttonLnF);
            btn.onClick = [this, tab] { setTab(tab); };
        };
        setupTabBtn(tabSerialButton, Tab::SerialKey);
        setupTabBtn(tabOfflineButton, Tab::OfflineFile);
        setupTabBtn(tabOnlineButton, Tab::OnlineAccount);

        // Method 1: Serial Controls
        addAndMakeVisible(serialEditor);
        serialEditor.setTextToShowWhenEmpty("PT-PROV-XXXX-XXXX-XXXX-XXXX", juce::Colour(0xff52525b));
        serialEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0b0d13));
        serialEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        serialEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff27272a));
        serialEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff00f0ff));
        serialEditor.setFont(juce::FontOptions("Consolas", 13.5f, juce::Font::bold));
        serialEditor.setJustification(juce::Justification::centred);

        addAndMakeVisible(activateSerialButton);
        activateSerialButton.setLookAndFeel(&buttonLnF);
        activateSerialButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00f0ff));
        activateSerialButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff05070a));
        activateSerialButton.onClick = [this] { handleActivateSerial(); };

        // Method 2: Offline File Controls
        addAndMakeVisible(copyDeviceIdButton);
        copyDeviceIdButton.setLookAndFeel(&buttonLnF);
        copyDeviceIdButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
        copyDeviceIdButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
        copyDeviceIdButton.onClick = [this]
        {
            juce::SystemClipboard::copyTextToClipboard(Licensing::LicenseClient::getInstance().getHardwareDeviceHash());
            statusMessage = "Device ID copied to clipboard.";
            isError = false;
            repaint();
        };

        addAndMakeVisible(importFileButton);
        importFileButton.setLookAndFeel(&buttonLnF);
        importFileButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00f0ff));
        importFileButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff05070a));
        importFileButton.onClick = [this] { handleImportOffline(); };

        // Method 3: Online Pairing Controls
        addAndMakeVisible(startPairingButton);
        startPairingButton.setLookAndFeel(&buttonLnF);
        startPairingButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00f0ff));
        startPairingButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff05070a));
        startPairingButton.onClick = [this] { handleStartPairing(); };

        addChildComponent(openBrowserButton);
        openBrowserButton.setLookAndFeel(&buttonLnF);
        openBrowserButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff22d3ee));
        openBrowserButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff05070a));
        openBrowserButton.onClick = [this]
        {
            if (activeClaimUrl.isNotEmpty())
                juce::URL(activeClaimUrl).launchInDefaultBrowser();
        };

        addChildComponent(cancelPairingButton);
        cancelPairingButton.setLookAndFeel(&buttonLnF);
        cancelPairingButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
        cancelPairingButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffa1a1aa));
        cancelPairingButton.onClick = [this]
        {
            Licensing::LicenseClient::getInstance().cancelPairingFlow();
            activePairingCode.clear();
            activeClaimUrl.clear();
            isWorking = false;
            statusMessage = "Pairing cancelled.";
            isError = false;
            resized();
            repaint();
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
        buyButton.setLookAndFeel(nullptr);
        deactivateButton.setLookAndFeel(nullptr);
        supportButton.setLookAndFeel(nullptr);

        tabSerialButton.setLookAndFeel(nullptr);
        tabOfflineButton.setLookAndFeel(nullptr);
        tabOnlineButton.setLookAndFeel(nullptr);

        activateSerialButton.setLookAndFeel(nullptr);
        copyDeviceIdButton.setLookAndFeel(nullptr);
        importFileButton.setLookAndFeel(nullptr);
        startPairingButton.setLookAndFeel(nullptr);
        openBrowserButton.setLookAndFeel(nullptr);
        cancelPairingButton.setLookAndFeel(nullptr);
    }

    juce::Rectangle<int> LicensePanel::getCardBounds() const
    {
        return getLocalBounds().withSizeKeepingCentre(state.isActivated ? 480 : 630, state.isActivated ? 300 : 420);
    }

    void LicensePanel::mouseDown(const juce::MouseEvent& event)
    {
        if (state.isActivated && !getCardBounds().contains(event.getPosition()) && onClose)
            onClose();
    }

    void LicensePanel::timerCallback()
    {
        if (isWorking && activePairingCode.isNotEmpty())
        {
            if (--pairingCountdown <= 0)
            {
                Licensing::LicenseClient::getInstance().cancelPairingFlow();
                activePairingCode.clear();
                isWorking = false;
                statusMessage = "Pairing session timed out. Please try again.";
                isError = true;
                resized();
                repaint();
                return;
            }
            repaint();
        }

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
        statusMessage = "License verified.";
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

    void LicensePanel::setTab(Tab tab)
    {
        currentTab = tab;
        statusMessage.clear();
        isError = false;
        resized();
        repaint();
    }

    void LicensePanel::refreshState()
    {
        state = Licensing::LicenseClient::getInstance().getCurrentState();
        resized();
        repaint();
    }

    void LicensePanel::handleActivateSerial()
    {
        const auto serial = serialEditor.getText().trim();
        if (serial.isEmpty())
        {
            statusMessage = "Please enter your serial key.";
            isError = true;
            repaint();
            return;
        }

        isWorking = true;
        statusMessage = "Contacting dedicated licensing server...";
        isError = false;
        repaint();

        juce::Component::SafePointer<LicensePanel> safeThis(this);
        Licensing::LicenseClient::getInstance().activateWithSerial(serial, [safeThis](bool success, const juce::String& msg)
        {
            if (auto* panel = safeThis.getComponent())
            {
                panel->isWorking = false;
                panel->statusMessage = msg;
                panel->isError = !success;
                panel->refreshState();

                if (success)
                {
                    juce::Timer::callAfterDelay(600, [safeThis]
                    {
                        if (auto* p = safeThis.getComponent())
                            if (p->state.isActivated && p->onClose)
                                p->onClose();
                    });
                }
            }
        });
    }

    void LicensePanel::handleImportOffline()
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

        statusMessage = "Choose your device-bound .ptlic file.";
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
                    panel->statusMessage = "License verified.";
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

    void LicensePanel::handleStartPairing()
    {
        isWorking = true;
        activePairingCode.clear();
        activeClaimUrl.clear();
        pairingCountdown = 300;
        statusMessage = "Connecting to dedicated licensing service...";
        isError = false;
        resized();
        repaint();

        juce::Component::SafePointer<LicensePanel> safeThis(this);
        Licensing::LicenseClient::getInstance().startOnlinePairing(
            [safeThis](bool ready, const juce::String& code, const juce::String& claimUrl, const juce::String& error)
            {
                if (auto* panel = safeThis.getComponent())
                {
                    if (ready)
                    {
                        panel->activePairingCode = code;
                        panel->activeClaimUrl = claimUrl;
                        panel->statusMessage = "Pairing code generated. Claim it in your browser.";
                        panel->isError = false;
                        panel->resized();
                        panel->repaint();
                    }
                    else
                    {
                        panel->isWorking = false;
                        panel->statusMessage = error;
                        panel->isError = true;
                        panel->resized();
                        panel->repaint();
                    }
                }
            },
            [safeThis](bool success, const juce::String& msg)
            {
                if (auto* panel = safeThis.getComponent())
                {
                    panel->isWorking = false;
                    panel->statusMessage = msg;
                    panel->isError = !success;
                    panel->activePairingCode.clear();
                    panel->activeClaimUrl.clear();
                    panel->refreshState();

                    if (success)
                    {
                        juce::Timer::callAfterDelay(600, [safeThis]
                        {
                            if (auto* p = safeThis.getComponent())
                                if (p->state.isActivated && p->onClose)
                                    p->onClose();
                        });
                    }
                }
            });
    }

    void LicensePanel::handleDeactivate()
    {
        Licensing::LicenseClient::getInstance().deactivate();
        refreshState();
        statusMessage = "License deactivated.";
        isError = false;
        repaint();

        if (onClose)
            onClose();
    }

    void LicensePanel::paint(juce::Graphics& g)
    {
        // Darkened backdrop with obsidian glow
        g.fillAll(juce::Colour(0xd008080a));
        auto card = getCardBounds().toFloat();

        // Card body - ProducerToy website charcoal theme
        g.setColour(juce::Colour(0xff121214));
        g.fillRoundedRectangle(card, 12.0f);
        g.setColour(juce::Colour(0xff27272a));
        g.drawRoundedRectangle(card, 12.0f, 1.0f);

        // Logo
        if (proverbLogoImage.isValid())
        {
            const float logoWidth = 110.0f;
            const float logoHeight = logoWidth * static_cast<float>(proverbLogoImage.getHeight())
                                                    / static_cast<float>(proverbLogoImage.getWidth());
            const float logoX = card.getCentreX() - logoWidth * 0.5f;
            const float logoY = card.getY() + (state.isActivated ? 22.0f : 18.0f);
            g.drawImage(proverbLogoImage, static_cast<int>(logoX), static_cast<int>(logoY),
                        static_cast<int>(logoWidth), static_cast<int>(logoHeight),
                        0, 0, proverbLogoImage.getWidth(), proverbLogoImage.getHeight());
        }

        const float contentX = card.getX() + 32.0f;
        const float contentW = card.getWidth() - 64.0f;

        if (state.isActivated)
        {
            float y = card.getY() + 70.0f;

            g.setFont(getPanelFont(11.0f));
            g.setColour(juce::Colour(0xff94a3b8));
            g.drawText("Smart Algorithmic Reverb", juce::roundToInt(contentX), juce::roundToInt(y), juce::roundToInt(contentW), 18, juce::Justification::centred);
            y += 20.0f;

            // Minimalist Version Tag Pill
            const juce::String versionText = "v1.0.0";
            g.setFont(getPanelFont(10.5f, juce::Font::bold));
            const int vW = 54;
            const int vH = 18;
            const float vX = card.getCentreX() - vW * 0.5f;
            g.setColour(juce::Colour(0xff18181b));
            g.fillRoundedRectangle(vX, y, (float)vW, (float)vH, 4.0f);
            g.setColour(juce::Colour(0xff27272a));
            g.drawRoundedRectangle(vX, y, (float)vW, (float)vH, 4.0f, 1.0f);
            g.setColour(juce::Colour(0xffa1a1aa));
            g.drawText(versionText, juce::roundToInt(vX), juce::roundToInt(y), vW, vH, juce::Justification::centred);
            y += 24.0f;

            g.setColour(juce::Colour(0xff12141a));
            g.fillRoundedRectangle(contentX, y, contentW, 76.0f, 6.0f);
            g.setColour(juce::Colour(0xff1e222d));
            g.drawRoundedRectangle(contentX, y, contentW, 76.0f, 6.0f, 1.0f);

            float infoY = y + 14.0f;
            g.setFont(getPanelFont(13.0f, juce::Font::bold));
            g.setColour(juce::Colours::white);
            g.drawText("Licensed to: " + state.registeredName, juce::roundToInt(contentX + 16.0f), juce::roundToInt(infoY), juce::roundToInt(contentW - 32.0f), 20, juce::Justification::centredLeft);
            infoY += 24.0f;

            g.setFont(getPanelFont(11.5f));
            g.setColour(juce::Colour(0xff8b949e));
            g.drawText("Account: " + state.maskedEmail, juce::roundToInt(contentX + 16.0f), juce::roundToInt(infoY), juce::roundToInt(contentW - 32.0f), 18, juce::Justification::centredLeft);
        }
        else
        {
            // Minimalist Version Tag in Unactivated / Trial State
            float subY = card.getY() + 64.0f;
            g.setFont(getPanelFont(10.5f));
            g.setColour(juce::Colour(0xff71717a));
            g.drawText("v1.0.0", juce::roundToInt(contentX), juce::roundToInt(subY), juce::roundToInt(contentW), 16, juce::Justification::centred);

            // Tab underline highlight
            float tabY = card.getY() + 88.0f;
            float tabW = (contentW - 16.0f) / 3.0f;
            float activeX = contentX;
            if (currentTab == Tab::OfflineFile) activeX += tabW + 8.0f;
            else if (currentTab == Tab::OnlineAccount) activeX += (tabW + 8.0f) * 2.0f;

            g.setColour(juce::Colour(0xff00f0ff));
            g.fillRect(activeX, tabY + 28.0f, tabW, 2.0f);

            float areaY = tabY + 44.0f;

            if (currentTab == Tab::SerialKey)
            {
                g.setFont(getPanelFont(12.5f, juce::Font::bold));
                g.setColour(juce::Colours::white);
                g.drawText("Enter Your Serial Key", juce::roundToInt(contentX), juce::roundToInt(areaY), juce::roundToInt(contentW), 20, juce::Justification::centredLeft);

                g.setFont(getPanelFont(11.0f));
                g.setColour(juce::Colour(0xff94a3b8));
                g.drawText("Serial keys are hashed securely on our dedicated server. Plaintext keys are never stored.",
                           juce::roundToInt(contentX), juce::roundToInt(areaY + 20.0f), juce::roundToInt(contentW), 16, juce::Justification::centredLeft);
            }
            else if (currentTab == Tab::OfflineFile)
            {
                g.setFont(getPanelFont(12.5f, juce::Font::bold));
                g.setColour(juce::Colours::white);
                g.drawText("Device-Bound Offline Activation (.ptlic)", juce::roundToInt(contentX), juce::roundToInt(areaY), juce::roundToInt(contentW), 20, juce::Justification::centredLeft);

                const auto devId = Licensing::LicenseClient::getInstance().getHardwareDeviceHash();
                const auto shortDev = devId.substring(0, juce::jmin(28, devId.length())) + "...";
                g.setFont(getPanelFont(11.0f));
                g.setColour(juce::Colour(0xff94a3b8));
                g.drawText("Machine Device ID: " + shortDev, juce::roundToInt(contentX), juce::roundToInt(areaY + 20.0f), juce::roundToInt(contentW), 16, juce::Justification::centredLeft);

                g.setFont(getPanelFont(10.0f));
                g.setColour(juce::Colour(0xff64748b));
                g.drawText("Notice: Offline license files are device-bound. Remote revocation & live seat enforcement are not available offline.",
                           juce::roundToInt(contentX), juce::roundToInt(areaY + 84.0f), juce::roundToInt(contentW), 16, juce::Justification::centredLeft);
            }
            else if (currentTab == Tab::OnlineAccount)
            {
                g.setFont(getPanelFont(12.5f, juce::Font::bold));
                g.setColour(juce::Colours::white);
                g.drawText("Pair With ProducerToy Account", juce::roundToInt(contentX), juce::roundToInt(areaY), juce::roundToInt(contentW), 20, juce::Justification::centredLeft);

                if (activePairingCode.isNotEmpty())
                {
                    g.setFont(juce::FontOptions("Consolas", 20.0f, juce::Font::bold));
                    g.setColour(juce::Colour(0xff00f0ff));
                    g.drawText(activePairingCode, juce::roundToInt(contentX), juce::roundToInt(areaY + 20.0f), juce::roundToInt(contentW), 28, juce::Justification::centredLeft);

                    g.setFont(getPanelFont(11.0f));
                    g.setColour(juce::Colour(0xffa1a1aa));
                    g.drawText("Expires in " + juce::String(pairingCountdown) + "s. Open browser to confirm authorization.",
                               juce::roundToInt(contentX), juce::roundToInt(areaY + 92.0f), juce::roundToInt(contentW), 16, juce::Justification::centredLeft);
                }
                else
                {
                    g.setFont(getPanelFont(11.0f));
                    g.setColour(juce::Colour(0xff94a3b8));
                    g.drawText("Pair this machine with your account. A short-lived, single-use pairing code will be generated.",
                               juce::roundToInt(contentX), juce::roundToInt(areaY + 20.0f), juce::roundToInt(contentW), 16, juce::Justification::centredLeft);
                }
            }

            // Status message positioned cleanly above bottom divider
            if (statusMessage.isNotEmpty())
            {
                g.setFont(getPanelFont(11.0f, isError ? juce::Font::bold : juce::Font::plain));
                g.setColour(isError ? juce::Colour(0xfff87171) : juce::Colour(0xff34d399));
                const float statusY = card.getBottom() - (state.isActivated ? 74.0f : 88.0f);
                g.drawFittedText(statusMessage, juce::Rectangle<int>(
                    static_cast<int>(contentX), static_cast<int>(statusY),
                    static_cast<int>(contentW), 18), juce::Justification::centredLeft, 1);
            }
        }
    }

    void LicensePanel::resized()
    {
        const auto card = getCardBounds();
        const int contentX = card.getX() + (state.isActivated ? 32 : 36);
        const int contentW = card.getWidth() - (state.isActivated ? 64 : 72);
        const int bottomBarY = card.getBottom() - (state.isActivated ? 42 : 44);

        closeButton.setBounds(card.getRight() - 30, card.getY() + 10, 20, 20);

        if (state.isActivated)
        {
            closeButton.setVisible(true);
            continueButton.setVisible(true);
            deactivateButton.setVisible(true);
            supportButton.setVisible(true);

            tabSerialButton.setVisible(false);
            tabOfflineButton.setVisible(false);
            tabOnlineButton.setVisible(false);
            serialEditor.setVisible(false);
            activateSerialButton.setVisible(false);
            copyDeviceIdButton.setVisible(false);
            importFileButton.setVisible(false);
            startPairingButton.setVisible(false);
            openBrowserButton.setVisible(false);
            cancelPairingButton.setVisible(false);
            buyButton.setVisible(false);

            auto bArea = juce::Rectangle<int>(contentX, bottomBarY, contentW, 26);
            continueButton.setBounds(bArea.removeFromLeft(100));
            bArea.removeFromLeft(12);
            deactivateButton.setBounds(bArea.removeFromLeft(110));
            bArea.removeFromLeft(12);
            supportButton.setBounds(bArea.removeFromLeft(100));
        }
        else
        {
            closeButton.setVisible(false);
            continueButton.setVisible(false);
            deactivateButton.setVisible(false);

            tabSerialButton.setVisible(true);
            tabOfflineButton.setVisible(true);
            tabOnlineButton.setVisible(true);
            buyButton.setVisible(true);
            supportButton.setVisible(true);

            // Tab bar layout
            const int tabY = card.getY() + 88;
            const int tabW = (contentW - 16) / 3;
            tabSerialButton.setBounds(contentX, tabY, tabW, 28);
            tabOfflineButton.setBounds(contentX + tabW + 8, tabY, tabW, 28);
            tabOnlineButton.setBounds(contentX + (tabW + 8) * 2, tabY, tabW, 28);

            // Tab button styles
            auto updateTabStyle = [this](juce::TextButton& btn, bool active)
            {
                btn.setColour(juce::TextButton::buttonColourId, active ? juce::Colour(0xff181c24) : juce::Colour(0xff0d0f14));
                btn.setColour(juce::TextButton::textColourOffId, active ? juce::Colour(0xff00f0ff) : juce::Colour(0xff71717a));
            };
            updateTabStyle(tabSerialButton, currentTab == Tab::SerialKey);
            updateTabStyle(tabOfflineButton, currentTab == Tab::OfflineFile);
            updateTabStyle(tabOnlineButton, currentTab == Tab::OnlineAccount);

            const int activeAreaY = tabY + 86;

            // Tab 1: Serial
            const bool isSerial = (currentTab == Tab::SerialKey);
            serialEditor.setVisible(isSerial);
            activateSerialButton.setVisible(isSerial);
            if (isSerial)
            {
                serialEditor.setBounds(contentX, activeAreaY, contentW - 160, 32);
                activateSerialButton.setBounds(contentX + contentW - 150, activeAreaY, 150, 32);
            }

            // Tab 2: Offline File
            const bool isOffline = (currentTab == Tab::OfflineFile);
            copyDeviceIdButton.setVisible(isOffline);
            importFileButton.setVisible(isOffline);
            if (isOffline)
            {
                copyDeviceIdButton.setBounds(contentX, activeAreaY, 140, 32);
                importFileButton.setBounds(contentX + 150, activeAreaY, 160, 32);
            }

            // Tab 3: Online Account
            const bool isOnline = (currentTab == Tab::OnlineAccount);
            startPairingButton.setVisible(isOnline && activePairingCode.isEmpty());
            openBrowserButton.setVisible(isOnline && activePairingCode.isNotEmpty());
            cancelPairingButton.setVisible(isOnline && activePairingCode.isNotEmpty());
            if (isOnline)
            {
                if (activePairingCode.isEmpty())
                {
                    startPairingButton.setBounds(contentX, activeAreaY, 180, 32);
                }
                else
                {
                    openBrowserButton.setBounds(contentX, tabY + 96, 150, 32);
                    cancelPairingButton.setBounds(contentX + 160, tabY + 96, 110, 32);
                }
            }

            // Footer buttons
            auto bArea = juce::Rectangle<int>(contentX, bottomBarY, contentW, 30);
            buyButton.setBounds(bArea.removeFromLeft(150));
            bArea.removeFromLeft(10);
            supportButton.setBounds(bArea.removeFromLeft(140));
        }
    }
}
