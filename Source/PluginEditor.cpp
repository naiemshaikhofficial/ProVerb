#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

ProVerbAudioProcessorEditor::ProVerbAudioProcessorEditor (ProVerbAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&customLaf);

    // Load ProducerToy Mascot & ProVerb Logos
    logoImage = juce::ImageFileFormat::loadFrom (BinaryData::producertoy_logo_png, BinaryData::producertoy_logo_pngSize);
    proverbLogoImage = juce::ImageFileFormat::loadFrom (BinaryData::proverb_logo_png, BinaryData::proverb_logo_pngSize);

    // Module 1: 3D Cylinder Visualizer
    addAndMakeVisible (visualizer);

    // Module 2: Filters & Routing
    addAndMakeVisible (midSideButton);
    midSideButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff9e9ea6));
    midSideButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffffffff));

    setupKnob (hCutSlider, hCutLabel, "H.CUT", " Hz", 14000.0);
    setupKnob (lCutSlider, lCutLabel, "L.CUT", " Hz", 80.0);
    setupKnob (duckSlider, duckLabel, "DUCK", " %", 0.0);

    // Module 2: Futuristic Sidechain Ducking Source Selector Button
    addAndMakeVisible (sidechainModeButton);
    sidechainModeButton.setButtonText ("SC: AUTO");
    sidechainModeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff14151a));
    sidechainModeButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff38bdf8));
    sidechainModeButton.setTooltip ("Sidechain Trigger: Auto (Smart AI) / Internal / External DAW Track");
    sidechainModeButton.onClick = [this]()
    {
        auto* p = audioProcessor.getAPVTS().getParameter ("duckingSource");
        if (p == nullptr) return;

        juce::PopupMenu menu;
        int curVal = static_cast<int> (p->getValue() * 2.0f + 0.5f);
        menu.addItem (1, "Auto (Smart AI) [Recommended]", true, curVal == 0);
        menu.addItem (2, "Internal (Dry Track)", true, curVal == 1);
        menu.addItem (3, "External (DAW Sidechain Bus)", true, curVal == 2);
        menu.addSeparator();

        bool hasScAudio = audioProcessor.isSidechainActive();
        bool hasScBus = audioProcessor.hasSidechainConnection();
        juce::String scStatus = hasScAudio ? "DAW Sidechain: ACTIVE SIGNAL DETECTED"
                                : (hasScBus ? "DAW Sidechain: Connected (Waiting for Audio)" : "DAW Sidechain: No Input Routed");
        menu.addItem (100, scStatus, false);

        juce::Component::SafePointer<ProVerbAudioProcessorEditor> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sidechainModeButton),
            [safeThis, p](int result)
            {
                if (result >= 1 && result <= 3)
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost ((result - 1) / 2.0f);
                    p->endChangeGesture();
                    if (safeThis != nullptr)
                        safeThis->resized();
                }
            });
    };

    // Module 3: Space & Time
    setupKnob (delaySlider, delayLabel, "DELAY", " ms", 20.0);
    setupKnob (sizeSlider, sizeLabel, "SIZE", " %", 70.0);
    setupKnob (diffSlider, diffLabel, "DIFF", " %", 75.0);
    setupKnob (shimmerSlider, shimmerLabel, "SHIM", " %", 0.0);

    // Module 4: Decay & Damping
    setupKnob (decSlider, decLabel, "DEC", " s", 2.8);
    setupKnob (dampSlider, dampLabel, "DAMP", " %", 45.0);

    addAndMakeVisible (freezeButton);
    freezeButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffffffff));
    freezeButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffffffff));

    // Module 5: Console Mixer Faders (INPUT, DRY, WET, OUTPUT) & SEP Knob
    setupFader (inGainFader, inGainLabel, "INPUT", 0.0);
    setupFader (dryFader, dryLabel, "DRY", 100.0);
    setupFader (wetFader, wetLabel, "WET", 35.0);
    setupFader (outGainFader, outGainLabel, "OUTPUT", 0.0);
    setupKnob (sepSlider, sepLabel, "SEP", " %", 100.0);

    // Top Bar - Categorized Professional Presets + User .toy Presets
    addAndMakeVisible (presetBox);
    rebuildPresetMenu();

    // Preset Instant Search Bar (Embedded inside Preset Selector)
    addChildComponent (searchBox);
    searchBox.setVisible (false);
    searchBox.setTextToShowWhenEmpty ("Search presets (Esc to close)...", juce::Colour (0xff6b7280));
    searchBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff14151a));
    searchBox.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3b82f6));
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff38bdf8));
    searchBox.setColour (juce::TextEditor::textColourId, juce::Colour (0xfff3f4f6));
    searchBox.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    searchBox.onTextChange = [this]() { performPresetSearch (searchBox.getText()); };
    searchBox.onEscapeKey  = [this]() { searchBox.setVisible (false); searchBox.clear(); };
    searchBox.onFocusLost  = [this]() { searchBox.setVisible (false); };
    searchBox.onReturnKey  = [this]()
    {
        const juce::String q = searchBox.getText().trim().toLowerCase();
        if (q.isNotEmpty())
        {
            auto matches = ProVerbPresets::searchPresets (0, q);
            if (! matches.empty())
            {
                int prog = matches[0];
                audioProcessor.setCurrentProgram (prog);
                lastMatchedPresetIndex = prog;
                presetBox.setSelectedId (prog + 1, juce::dontSendNotification);
                searchBox.setVisible (false);
                searchBox.clear();
            }
        }
    };

    // A/B Comparison Buttons
    addAndMakeVisible (abButton);
    abButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1e293b));
    abButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff38bdf8));
    abButton.setTooltip ("A/B Comparison: Toggle between Reverb State A and State B");
    captureSnapshot (snapshotA);
    snapshotB = snapshotA;

    abButton.onClick = [this]()
    {
        if (isStateBActive)
        {
            captureSnapshot (snapshotB);
            applySnapshot (snapshotA);
            isStateBActive = false;
            abButton.setButtonText ("A");
            abButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1e293b));
            abButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff38bdf8));
            copyABButton.setButtonText ("A->B");
        }
        else
        {
            captureSnapshot (snapshotA);
            if (snapshotB.values.empty())
                snapshotB = snapshotA;
            applySnapshot (snapshotB);
            isStateBActive = true;
            abButton.setButtonText ("B");
            abButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2e1065));
            abButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffc084fc));
            copyABButton.setButtonText ("B->A");
        }
    };

    addAndMakeVisible (copyABButton);
    copyABButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff14151a));
    copyABButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff94a3b8));
    copyABButton.setTooltip ("Copy current reverb state to opposite slot");
    copyABButton.onClick = [this]()
    {
        juce::Component::SafePointer<ProVerbAudioProcessorEditor> safeThis (this);
        if (! isStateBActive)
        {
            captureSnapshot (snapshotA);
            snapshotB = snapshotA;
            copyABButton.setButtonText ("COPIED");
            juce::Timer::callAfterDelay (800, [safeThis]()
            {
                if (safeThis != nullptr)
                    safeThis->copyABButton.setButtonText ("A->B");
            });
        }
        else
        {
            captureSnapshot (snapshotB);
            snapshotA = snapshotB;
            copyABButton.setButtonText ("COPIED");
            juce::Timer::callAfterDelay (800, [safeThis]()
            {
                if (safeThis != nullptr)
                    safeThis->copyABButton.setButtonText ("B->A");
            });
        }
    };

    audioProcessor.updateLicenseStatus();

    // Vector GUI Scale Button (100% / 125% / 150%)
    addAndMakeVisible (scaleButton);
    scaleButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff14151a));
    scaleButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff94a3b8));
    scaleButton.setTooltip ("Vector GUI Scale: 100%, 125%, 150%");
    scaleButton.onClick = [this]()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "100%  (Standard 1120x640)", true, currentScaleFactor == 1.0f);
        menu.addItem (2, "125%  (Medium 1400x800)", true, currentScaleFactor == 1.25f);
        menu.addItem (3, "150%  (Large 1680x960)", true, currentScaleFactor == 1.5f);
        juce::Component::SafePointer<ProVerbAudioProcessorEditor> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&scaleButton),
            [safeThis](int result)
            {
                if (safeThis == nullptr) return;
                if (result == 1) safeThis->setGuiScale (1.0f);
                else if (result == 2) safeThis->setGuiScale (1.25f);
                else if (result == 3) safeThis->setGuiScale (1.5f);
            });
    };

    addAndMakeVisible (bypassButton);
    bypassButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff888890));
    bypassButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff4444));

    // Smart AI Auto Learn Button with Real-Time Animated Waveform & Sticky Badge
    addAndMakeVisible (smartAutoLearnButton);
    auto triggerAnalysis = [this]()
    {
        if (smartAutoLearnButton.isCurrentlyAnalyzing())
        {
            // If already analyzing, click cancels safely
            audioProcessor.abortAutoLearn();
            smartAutoLearnButton.setAnalyzing (false, 0.0f);
            visualizer.setAnalyzing (false, 0.0f);
            modernEQ.setAnalyzing (false, 0.0f);
            wasAnalyzing = false;
        }
        else
        {
            audioProcessor.triggerAutoLearn();
            smartAutoLearnButton.setAnalyzing (true, 0.0f, audioProcessor.getRmsInput());
            visualizer.setAnalyzing (true, 0.0f);
            modernEQ.setAnalyzing (true, 0.0f);
            wasAnalyzing = true;
            analysisWatchdogTicks = 0;
        }
    };
    smartAutoLearnButton.onClick = triggerAnalysis;
    smartAutoLearnButton.onReanalyzeClick = triggerAnalysis;

    addChildComponent (undoAutoLearnButton);
    undoAutoLearnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b1414));
    undoAutoLearnButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff7777));
    undoAutoLearnButton.setTooltip ("Revert / Undo Auto Learn and restore previous mix");
    undoAutoLearnButton.onClick = [this]()
    {
        audioProcessor.undoAutoLearn();
        undoAutoLearnButton.setVisible (false);
        smartAutoLearnButton.setLearnedResult ("", "", false);
        const int curProg = audioProcessor.getCurrentProgram();
        presetBox.setSelectedId (curProg + 1, juce::dontSendNotification);
        resized();
    };

    audioProcessor.onAutoLearnResult = [this](const juce::String& inst, const juce::String& prof, bool success)
    {
        wasAnalyzing = false;
        analysisWatchdogTicks = 0;
        visualizer.setAnalyzing (false, 0.0f);
        modernEQ.setAnalyzing (false, 0.0f);
        smartAutoLearnButton.setLearnedResult (inst, prof, success);
        if (success && prof.isNotEmpty())
        {
            presetBox.setText ("AI: " + prof, juce::dontSendNotification);
            undoAutoLearnButton.setVisible (true);
        }
        else
        {
            undoAutoLearnButton.setVisible (false);
        }
        resized();
    };

    if (audioProcessor.hasAutoLearned())
    {
        smartAutoLearnButton.setLearnedResult (audioProcessor.getDetectedInstrument(),
                                               audioProcessor.getDetectedProfileName(),
                                               true);
        if (audioProcessor.getDetectedProfileName().isNotEmpty())
            presetBox.setText ("AI: " + audioProcessor.getDetectedProfileName(), juce::dontSendNotification);
        undoAutoLearnButton.setVisible (audioProcessor.canUndoAutoLearn());
    }

    // Connect to Processor Parameters (APVTS)
    auto& apvts = audioProcessor.getAPVTS();
    delayAttach   = std::make_unique<SliderAttachment> (apvts, "predelayMs", delaySlider);
    sizeAttach    = std::make_unique<SliderAttachment> (apvts, "size", sizeSlider);
    decAttach     = std::make_unique<SliderAttachment> (apvts, "decaySeconds", decSlider);
    diffAttach    = std::make_unique<SliderAttachment> (apvts, "diffusion", diffSlider);
    dampAttach    = std::make_unique<SliderAttachment> (apvts, "damping", dampSlider);
    hCutAttach    = std::make_unique<SliderAttachment> (apvts, "highCut", hCutSlider);
    lCutAttach    = std::make_unique<SliderAttachment> (apvts, "lowCut", lCutSlider);
    duckAttach    = std::make_unique<SliderAttachment> (apvts, "ducking", duckSlider);
    shimmerAttach = std::make_unique<SliderAttachment> (apvts, "shimmer", shimmerSlider);
    sepAttach     = std::make_unique<SliderAttachment> (apvts, "width", sepSlider);
    inGainAttach  = std::make_unique<SliderAttachment> (apvts, "inputGain", inGainFader);
    dryAttach     = std::make_unique<SliderAttachment> (apvts, "dry", dryFader);
    wetAttach     = std::make_unique<SliderAttachment> (apvts, "wet", wetFader);
    outGainAttach = std::make_unique<SliderAttachment> (apvts, "outputGain", outGainFader);
    freezeAttach  = std::make_unique<ButtonAttachment> (apvts, "freeze", freezeButton);
    bypassAttach  = std::make_unique<ButtonAttachment> (apvts, "bypass", bypassButton);
    midSideAttach = std::make_unique<ButtonAttachment> (apvts, "midSide", midSideButton);

    // Module 6: Modern Interactive Visual EQ & Real-Time Spectrum
    addAndMakeVisible (modernEQ);
    modernEQ.onLowCutChanged = [this](float hz)
    {
        lCutSlider.setValue (hz, juce::sendNotificationSync);
    };
    modernEQ.onHighCutChanged = [this](float hz)
    {
        hCutSlider.setValue (hz, juce::sendNotificationSync);
    };

    // Expanded Plugin Dimensions for Neoverb-style Centerpiece & Visual EQ
    setSize (1120, 640);
    setResizable (true, true);
    setResizeLimits (960, 550, 1680, 960);

    // If not activated, immediately show mandatory activation dialog on open
    if (! ProducerToy::Licensing::LicenseClient::getInstance().isLicenseValid())
    {
        showLicensePanel();
    }

    startTimerHz (30);
}

ProVerbAudioProcessorEditor::~ProVerbAudioProcessorEditor()
{
    audioProcessor.onAutoLearnResult = nullptr;
    audioProcessor.onProgramChange = nullptr;
    stopTimer();
    setLookAndFeel (nullptr);
}

void ProVerbAudioProcessorEditor::setupKnob (juce::Slider& slider, juce::Label& label,
                                             const juce::String& text, const juce::String& suffix, double defaultVal)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 16);
    slider.setTextValueSuffix (suffix);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x00000000));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe2e8f0));
    slider.setDoubleClickReturnValue (true, defaultVal);
    slider.addMouseListener (&contextMenuHelper, false);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff8a90a2));
    addAndMakeVisible (label);
}

void ProVerbAudioProcessorEditor::setupFader (juce::Slider& slider, juce::Label& label, const juce::String& text, double defaultVal)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 16);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x18ffffff));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0e0f14));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe2e8f0));
    slider.setDoubleClickReturnValue (true, defaultVal);
    slider.addMouseListener (&contextMenuHelper, false);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff8a90a2));
    addAndMakeVisible (label);
}

void ProVerbAudioProcessorEditor::timerCallback()
{
    bool currentlyAnalyzing = audioProcessor.isAutoLearning();
    float analyzeProg = audioProcessor.getAutoLearnProgress();

    if (currentlyAnalyzing)
    {
        wasAnalyzing = true;
        analysisWatchdogTicks++;

        // Feed real-time progress and live RMS to button for dynamic oscillating waveform
        smartAutoLearnButton.setAnalyzing (true, analyzeProg, audioProcessor.getRmsInput());
        visualizer.setAnalyzing (true, analyzeProg);
        modernEQ.setAnalyzing (true, analyzeProg);

        // Safety watchdog: If host transport is paused and ~6 seconds elapse without audio buffers
        if (analysisWatchdogTicks > 180) // 180 * 33ms = ~6.0s
        {
            audioProcessor.abortAutoLearn();
            visualizer.setAnalyzing (false, 0.0f);
            modernEQ.setAnalyzing (false, 0.0f);
            smartAutoLearnButton.setLearnedResult ("NO AUDIO", "Play Track in DAW", false);
            wasAnalyzing = false;
            analysisWatchdogTicks = 0;
        }
    }
    else
    {
        visualizer.setAnalyzing (false, 0.0f);
        modernEQ.setAnalyzing (false, 0.0f);

        if (wasAnalyzing)
        {
            wasAnalyzing = false;
            analysisWatchdogTicks = 0;
            juce::String inst = audioProcessor.getDetectedInstrument();
            juce::String prof = audioProcessor.getDetectedProfileName();

            if (inst == "NO AUDIO" || prof.startsWith ("No Audio"))
            {
                smartAutoLearnButton.setLearnedResult ("NO AUDIO", "Play Track in DAW", false);
            }
            else if (inst.isNotEmpty())
            {
                smartAutoLearnButton.setLearnedResult (inst, prof, true);
                if (prof.isNotEmpty())
                    presetBox.setText ("AI: " + prof, juce::dontSendNotification);
            }
        }
    }

    // Dynamic Custom Preset Tracking: When user moves any knob, display "Custom" if it differs from known factory presets
    if (! currentlyAnalyzing && ! audioProcessor.isAutoLearning())
    {
        const int match = ProVerbPresets::findMatchingPresetIndex (audioProcessor.getAPVTS());
        if (match != lastMatchedPresetIndex)
        {
            lastMatchedPresetIndex = match;
            if (match >= 0)
            {
                presetBox.setSelectedId (match + 1, juce::dontSendNotification);
            }
            else
            {
                presetBox.setSelectedId (0, juce::dontSendNotification);
                presetBox.setText ("Custom", juce::dontSendNotification);
            }
        }
    }

    smartAutoLearnButton.advanceAnimation (0.05f);

    // Real-time Sidechain status & glowing LED activity update
    auto* scParam = audioProcessor.getAPVTS().getRawParameterValue ("duckingSource");
    const int scMode = scParam != nullptr ? static_cast<int> (scParam->load()) : 0;
    const bool isScActive = audioProcessor.isSidechainActive();
    if (scMode == 0)
        sidechainModeButton.setButtonText (isScActive ? "● SC: AUTO" : "SC: AUTO");
    else if (scMode == 1)
        sidechainModeButton.setButtonText ("SC: INT");
    else
        sidechainModeButton.setButtonText (isScActive ? "● SC: EXT" : "SC: EXT");

    if (isScActive)
    {
        sidechainModeButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff00f0ff));
        sidechainModeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0c2738));
    }
    else
    {
        sidechainModeButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff94a3b8));
        sidechainModeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff14151a));
    }

    visualizer.updateAllParameters (audioProcessor.getRmsInput(),
                                    audioProcessor.getRmsOutput(),
                                    (float) sizeSlider.getValue(),
                                    (float) decSlider.getValue(),
                                    (float) diffSlider.getValue(),
                                    (float) dampSlider.getValue(),
                                    (float) delaySlider.getValue(),
                                    (float) shimmerSlider.getValue(),
                                    audioProcessor.getStereoPhaseCorrelation());
    visualizer.advanceAnimation (0.05f);

    modernEQ.updateParameters ((float) lCutSlider.getValue(), (float) hCutSlider.getValue(), audioProcessor.getRmsOutput());
    modernEQ.advanceAnimation (0.05f);
}

void ProVerbAudioProcessorEditor::paint (juce::Graphics& g)
{
    // 1. Futuristic Deep OLED Black Canvas
    g.fillAll (juce::Colour (0xff08090c));

    // Outer Precision Cyber Rim
    g.setColour (juce::Colour (0xff1c1f2b));
    g.drawRect (getLocalBounds(), 1);

    // 2. Futuristic Top Header Bar
    auto headerBounds = getLocalBounds().removeFromTop (52);
    g.setColour (juce::Colour (0xff0a0b0f));
    g.fillRect (headerBounds);

    // Sleek hairline cyber divider with subtle cyan glow on left
    g.setColour (juce::Colour (0xff1c1f2b));
    g.drawHorizontalLine (52, 0.0f, (float) getWidth());
    juce::ColourGradient headerLineGrad (juce::Colour (0x5500f0ff), 0.0f, 52.0f,
                                         juce::Colour (0x0000f0ff), (float) getWidth() * 0.4f, 52.0f, false);
    g.setGradientFill (headerLineGrad);
    g.drawHorizontalLine (52, 0.0f, (float) getWidth() * 0.4f);

    // Enable Studio High-Quality Resampling for razor-sharp, vector-grade logo rendering
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    // Main Logo: Official Bold ProVerb Wordmark
    if (proverbLogoCached.isValid())
    {
        g.drawImage (proverbLogoCached, proverbLogoBounds.toFloat(),
                     juce::RectanglePlacement (juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize));
    }
    else if (proverbLogoImage.isValid())
    {
        g.drawImageWithin (proverbLogoImage, proverbLogoBounds.getX(), proverbLogoBounds.getY(),
                           proverbLogoBounds.getWidth(), proverbLogoBounds.getHeight(),
                           juce::RectanglePlacement (juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize));
    }
    else
    {
        g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xffffffff));
        g.drawText ("PROVERB", 14, 0, 130, 52, juce::Justification::centredLeft);
    }

    // Elegant Divider (Subtle Cyber Line)
    g.setColour (juce::Colour (0xff252936));
    g.drawVerticalLine (headerDividerX, 16.0f, 36.0f);

    // Secondary Brand: ProducerToy Badge
    if (ptLogoCached.isValid())
    {
        g.drawImage (ptLogoCached, ptLogoBounds.toFloat(),
                     juce::RectanglePlacement (juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize));
    }
    else if (logoImage.isValid())
    {
        g.drawImageWithin (logoImage, ptLogoBounds.getX(), ptLogoBounds.getY(),
                           ptLogoBounds.getWidth(), ptLogoBounds.getHeight(),
                           juce::RectanglePlacement (juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize));
    }

    // 4. Middle Workspace Modular Cards (Futuristic Minimalist Obsidian Panels)
    int eqH = juce::jlimit (135, 210, (int) (getHeight() * 0.28f));
    const int cardTopY = 56;
    const int cardBottomY = getHeight() - eqH - 6;
    const int cardH = cardBottomY - cardTopY;

    float col1W = (float) getWidth() * 0.16f;
    float col2W = (float) getWidth() * 0.19f;
    float col3W = (float) getWidth() * 0.28f;
    float col4W = (float) getWidth() * 0.17f;

    float x1 = col1W;
    float x2 = x1 + col2W;
    float x3 = x2 + col3W;
    float x4 = x3 + col4W;

    // Helper to draw modular futuristic cards
    auto drawModuleCard = [&](float cx, float cw, const char* title)
    {
        auto cardRect = juce::Rectangle<float> (cx + 3.0f, (float) cardTopY, cw - 6.0f, (float) cardH);

        // 1. Obsidian Glass Panel Background
        juce::ColourGradient cardGrad (juce::Colour (0xff0e1016), cx, (float) cardTopY,
                                       juce::Colour (0xff090a0e), cx, (float) (cardTopY + cardH), false);
        g.setGradientFill (cardGrad);
        g.fillRoundedRectangle (cardRect, 7.0f);

        // 2. Razor-Thin Cyber Border
        g.setColour (juce::Colour (0xff1c202c));
        g.drawRoundedRectangle (cardRect, 7.0f, 1.0f);

        // 3. Subtle Top Edge Highlight
        g.setColour (juce::Colour (0x18ffffff));
        g.drawHorizontalLine ((int) cardRect.getY() + 1, cardRect.getX() + 6.0f, cardRect.getRight() - 6.0f);

        // 4. Futuristic Minimalist Title Header
        float titleY = cardRect.getY() + 5.0f;
        float titleH = 16.0f;

        // Glowing Neon Cyan Micro-Pip
        g.setColour (juce::Colour (0x4400f0ff));
        g.fillEllipse (cardRect.getX() + 10.0f, titleY + 4.5f, 6.0f, 6.0f);
        g.setColour (juce::Colour (0xff00f0ff));
        g.fillEllipse (cardRect.getX() + 11.5f, titleY + 6.0f, 3.0f, 3.0f);

        // Title Text — Crisp modern typography
        float titleFontSize = (cw < 150.0f) ? 8.5f : 9.0f;
        g.setFont (juce::FontOptions (titleFontSize, juce::Font::bold));
        g.setColour (juce::Colour (0xff9aa0b2));
        auto titleBounds = juce::Rectangle<float> (cardRect.getX() + 20.0f, titleY, cardRect.getWidth() - 26.0f, titleH);
        g.drawText (title, titleBounds.toNearestInt(), juce::Justification::centredLeft, true);

        // 5. Minimalist Hairline Separator with subtle cyan accent fade
        float lineY = cardRect.getY() + 24.0f;
        g.setColour (juce::Colour (0xff181a24));
        g.drawHorizontalLine ((int) lineY, cardRect.getX() + 8.0f, cardRect.getRight() - 8.0f);
        
        juce::ColourGradient lineAccent (juce::Colour (0x4400f0ff), cardRect.getX() + 8.0f, lineY,
                                         juce::Colour (0x0000f0ff), cardRect.getX() + 80.0f, lineY, false);
        g.setGradientFill (lineAccent);
        g.drawHorizontalLine ((int) lineY, cardRect.getX() + 8.0f, cardRect.getX() + 80.0f);
    };

    drawModuleCard (0.0f, col1W, "ROUTING & FILTERS");
    drawModuleCard (x1, col2W, "SPACE & TIME");
    drawModuleCard (x2, col3W, "ACOUSTIC AURA");
    drawModuleCard (x3, col4W, "DECAY & DAMP");
    drawModuleCard (x4, (float) getWidth() - x4, "CONSOLE MIXER");
}

void ProVerbAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Top Header (52px)
    auto headerArea = area.removeFromTop (52);

    // Header logos - calculate exact pixel bounds and generate high-DPI supersampled cache
    const int logoH = 34;
    int logoW = 132;
    if (proverbLogoImage.isValid() && proverbLogoImage.getHeight() > 0)
        logoW = (int) std::round (logoH * ((float) proverbLogoImage.getWidth() / (float) proverbLogoImage.getHeight()));

    proverbLogoBounds = juce::Rectangle<int> (14, 9, logoW, logoH);
    headerDividerX = proverbLogoBounds.getRight() + 10;

    const int ptLogoH = 26;
    int ptLogoW = 76;
    if (logoImage.isValid() && logoImage.getHeight() > 0)
        ptLogoW = (int) std::round (ptLogoH * ((float) logoImage.getWidth() / (float) logoImage.getHeight()));

    ptLogoBounds = juce::Rectangle<int> (headerDividerX + 10, 13, ptLogoW, ptLogoH);

    // Pre-downscale to 2x using high-quality bicubic resampling for razor-sharp Retina/4K clarity
    if (proverbLogoImage.isValid())
    {
        int targetW = logoW * 2;
        int targetH = logoH * 2;
        if (! proverbLogoCached.isValid() || proverbLogoCached.getWidth() != targetW || proverbLogoCached.getHeight() != targetH)
            proverbLogoCached = proverbLogoImage.rescaled (targetW, targetH, juce::Graphics::highResamplingQuality);
    }

    if (logoImage.isValid())
    {
        int targetW = ptLogoW * 2;
        int targetH = ptLogoH * 2;
        if (! ptLogoCached.isValid() || ptLogoCached.getWidth() != targetW || ptLogoCached.getHeight() != targetH)
            ptLogoCached = logoImage.rescaled (targetW, targetH, juce::Graphics::highResamplingQuality);
    }

    // Right Side: Top Bar Controls (Right-to-Left Layout)
    const int rightMargin = 12;
    const int headerGap = 6;
    const int headerY = 11;
    const int ctrlH = 30;

    int curX = headerArea.getRight() - rightMargin;


    // 1. GUI Scale Button (100% / 125% / 150%)
    const int scaleW = 56;
    curX -= scaleW;
    scaleButton.setBounds (curX, headerY, scaleW, ctrlH);
    curX -= headerGap;

    // 2. A/B Copy Button
    const int copyW = 50;
    curX -= copyW;
    copyABButton.setBounds (curX, headerY, copyW, ctrlH);
    curX -= 4;

    // 3. A/B State Toggle Button
    const int abW = 34;
    curX -= abW;
    abButton.setBounds (curX, headerY, abW, ctrlH);
    curX -= headerGap;

    // 4. Preset Selector Dropdown (Spacious & Prominent with Integrated Search)
    const int presetW = 210;
    curX -= presetW;
    presetBox.setBounds (curX, headerY, presetW, ctrlH);
    searchBox.setBounds (curX, headerY, presetW, ctrlH);
    curX -= headerGap + 8;

    // 5. Bypass Button
    const int bypassW = 76;
    curX -= bypassW;
    bypassButton.setBounds (curX, headerY + 1, bypassW, ctrlH - 2);

    // Center: iZotope-Style Reverb Assistant — Responsively centered between left brands & right controls
    int leftBrandRight = ptLogoBounds.getRight();
    const bool showUndo = undoAutoLearnButton.isVisible();
    const int undoW = showUndo ? 52 : 0;
    int centerAvailW = (curX - 10) - (leftBrandRight + 12);
    int assistantW = juce::jlimit (170, 220, centerAvailW - (showUndo ? (undoW + 6) : 0));
    int assistantH = 32;
    int totalBlockW = assistantW + (showUndo ? (undoW + 6) : 0);
    int assistantX = (leftBrandRight + 12) + (centerAvailW - totalBlockW) / 2;
    int assistantY = (headerArea.getHeight() - assistantH) / 2;
    smartAutoLearnButton.setBounds (assistantX, assistantY, assistantW, assistantH);

    if (showUndo)
        undoAutoLearnButton.setBounds (assistantX + assistantW + 6, assistantY + 2, undoW, assistantH - 4);

    // Bottom Reverb EQ & Spectrum Module — dynamically scales with window height
    int eqH = juce::jlimit (135, 210, (int) (getHeight() * 0.28f));
    auto eqArea = area.removeFromBottom (eqH).reduced (8, 4);
    modernEQ.setBounds (eqArea);

    // 5 Middle Workspace Columns
    int totalW = getWidth();
    int col1W = (int) (totalW * 0.16f); // Filters (H.CUT, L.CUT, DUCK, MID/SIDE)
    int col2W = (int) (totalW * 0.19f); // Space & Time (DELAY, SIZE, DIFF, SHIMMER)
    int col3W = (int) (totalW * 0.28f); // Center Holographic Aura & 3D SpaceVisualizer
    int col4W = (int) (totalW * 0.17f); // Decay & Damping (DEC, DAMP, FREEZE)
    int col5W = totalW - (col1W + col2W + col3W + col4W); // Console Mixer
    juce::ignoreUnused (col5W);

    auto m1Area = area.removeFromLeft (col1W);
    auto m2Area = area.removeFromLeft (col2W);
    auto m3Area = area.removeFromLeft (col3W);
    auto m4Area = area.removeFromLeft (col4W);
    auto m5Area = area;

    // Card dimensions matching paint()
    const int cardTopY = 56;
    const int cardBottomY = getHeight() - eqH - 6;
    const int contentTopY = cardTopY + 28;
    const int contentBottomY = cardBottomY - 6;
    const int availableH = contentBottomY - contentTopY;

    // Module 1 (Left 1): MID/SIDE & Filters (H.CUT, L.CUT, DUCK)
    int m1BtnH = 22;
    midSideButton.setBounds (m1Area.getX() + 12, contentTopY, m1Area.getWidth() - 24, m1BtnH);

    int m1KnobsStart = contentTopY + m1BtnH + 4;
    int m1KnobsHeight = contentBottomY - m1KnobsStart;
    int m1SlotH = m1KnobsHeight / 3;
    int m1LabelH = 13;
    int m1KnobDiam = juce::jlimit (26, 48, juce::jmin (m1Area.getWidth() - 20, m1SlotH - m1LabelH - 14));
    int m1SliderH = m1KnobDiam + 14;

    // Row 1: H.CUT
    int m1SliderW = juce::jmin (m1Area.getWidth() - 10, juce::jmax (68, m1KnobDiam + 24));
    int y1_1 = m1KnobsStart;
    int pad1_1 = (m1SlotH - (m1LabelH + m1SliderH)) / 2;
    hCutLabel.setBounds (m1Area.getX(), y1_1 + pad1_1, m1Area.getWidth(), m1LabelH);
    hCutSlider.setBounds (juce::Rectangle<int> (m1Area.getX(), y1_1 + pad1_1 + m1LabelH, m1Area.getWidth(), m1SliderH)
                             .withSizeKeepingCentre (m1SliderW, m1SliderH));

    // Row 2: L.CUT
    int y1_2 = m1KnobsStart + m1SlotH;
    int pad1_2 = (m1SlotH - (m1LabelH + m1SliderH)) / 2;
    lCutLabel.setBounds (m1Area.getX(), y1_2 + pad1_2, m1Area.getWidth(), m1LabelH);
    lCutSlider.setBounds (juce::Rectangle<int> (m1Area.getX(), y1_2 + pad1_2 + m1LabelH, m1Area.getWidth(), m1SliderH)
                             .withSizeKeepingCentre (m1SliderW, m1SliderH));

    // Row 3: DUCK & Sidechain Selector
    int y1_3 = m1KnobsStart + m1SlotH * 2;
    int pad1_3 = (m1SlotH - (m1LabelH + m1SliderH + 18)) / 2;
    duckLabel.setBounds (m1Area.getX(), y1_3 + pad1_3, m1Area.getWidth(), m1LabelH);
    duckSlider.setBounds (juce::Rectangle<int> (m1Area.getX(), y1_3 + pad1_3 + m1LabelH, m1Area.getWidth(), m1SliderH)
                             .withSizeKeepingCentre (m1SliderW, m1SliderH));
    sidechainModeButton.setBounds (juce::Rectangle<int> (m1Area.getX(), y1_3 + pad1_3 + m1LabelH + m1SliderH + 2, m1Area.getWidth(), 16)
                                      .withSizeKeepingCentre (juce::jmin (m1Area.getWidth() - 10, 68), 15));

    // Module 2 (Left 2): Space & Time (DELAY, SIZE, DIFF, SHIMMER)
    int m2KnobsHeight = contentBottomY - contentTopY;
    int m2SlotH = m2KnobsHeight / 4;
    int m2LabelH = 12;
    int m2KnobDiam = juce::jlimit (24, 44, juce::jmin (m2Area.getWidth() - 16, m2SlotH - m2LabelH - 14));
    int m2SliderH = m2KnobDiam + 14;
    int m2SliderW = juce::jmin (m2Area.getWidth() - 10, juce::jmax (68, m2KnobDiam + 24));

    auto layoutM2Knob = [&](int index, juce::Label& lbl, juce::Slider& sld)
    {
        int slotY = contentTopY + index * m2SlotH;
        int pad = (m2SlotH - (m2LabelH + m2SliderH)) / 2;
        lbl.setBounds (m2Area.getX(), slotY + pad, m2Area.getWidth(), m2LabelH);
        sld.setBounds (juce::Rectangle<int> (m2Area.getX(), slotY + pad + m2LabelH, m2Area.getWidth(), m2SliderH)
                           .withSizeKeepingCentre (m2SliderW, m2SliderH));
    };

    layoutM2Knob (0, delayLabel, delaySlider);
    layoutM2Knob (1, sizeLabel, sizeSlider);
    layoutM2Knob (2, diffLabel, diffSlider);
    layoutM2Knob (3, shimmerLabel, shimmerSlider);

    // Module 3 (Center): Centerpiece Holographic Acoustic Aura Visualizer
    visualizer.setBounds (m3Area.getX() + 5, contentTopY + 2, m3Area.getWidth() - 10, contentBottomY - contentTopY - 4);

    // Module 4 (Right 1): Decay & Damping (DEC, DAMP, FREEZE)
    int freezeH = 24;
    int m4KnobsAreaH = availableH - freezeH - 8;
    int m4SlotH = m4KnobsAreaH / 2;
    int m4LabelH = 13;
    int m4KnobDiam = juce::jlimit (28, 54, juce::jmin (m4Area.getWidth() - 20, m4SlotH - m4LabelH - 15));
    int m4SliderH = m4KnobDiam + 15;
    int m4SliderW = juce::jmin (m4Area.getWidth() - 10, juce::jmax (68, m4KnobDiam + 24));

    // Item 1: DEC (Hero Decay Dial)
    int y4_1 = contentTopY;
    int pad4_1 = (m4SlotH - (m4LabelH + m4SliderH)) / 2;
    decLabel.setBounds (m4Area.getX(), y4_1 + pad4_1, m4Area.getWidth(), m4LabelH);
    decSlider.setBounds (juce::Rectangle<int> (m4Area.getX(), y4_1 + pad4_1 + m4LabelH, m4Area.getWidth(), m4SliderH)
                            .withSizeKeepingCentre (m4SliderW, m4SliderH));

    // Item 2: DAMP (High Damping Dial)
    int y4_2 = contentTopY + m4SlotH;
    int pad4_2 = (m4SlotH - (m4LabelH + m4SliderH)) / 2;
    dampLabel.setBounds (m4Area.getX(), y4_2 + pad4_2, m4Area.getWidth(), m4LabelH);
    dampSlider.setBounds (juce::Rectangle<int> (m4Area.getX(), y4_2 + pad4_2 + m4LabelH, m4Area.getWidth(), m4SliderH)
                             .withSizeKeepingCentre (m4SliderW, m4SliderH));

    // Item 3: FREEZE Button
    freezeButton.setBounds (juce::Rectangle<int> (m4Area.getX(), contentBottomY - freezeH - 2, m4Area.getWidth(), freezeH)
                               .withSizeKeepingCentre (juce::jmin (m4Area.getWidth() - 24, 96), freezeH));

    // Module 5 (Right 2): Console Mixer Faders (INPUT, DRY, WET, OUTPUT) & SEP Knob
    int sepSectionH = juce::jlimit (62, 78, (int) (availableH * 0.28f));
    int faderSectionH = availableH - sepSectionH - 6;

    auto fadersArea = juce::Rectangle<int> (m5Area.getX() + 4, contentTopY + 2, m5Area.getWidth() - 8, faderSectionH);
    int faderColW = fadersArea.getWidth() / 4;

    auto inCol = fadersArea.removeFromLeft (faderColW);
    inGainLabel.setBounds (inCol.removeFromBottom (13));
    inCol.removeFromBottom (2);
    inGainFader.setBounds (inCol);

    auto dryCol = fadersArea.removeFromLeft (faderColW);
    dryLabel.setBounds (dryCol.removeFromBottom (13));
    dryCol.removeFromBottom (2);
    dryFader.setBounds (dryCol);

    auto wetCol = fadersArea.removeFromLeft (faderColW);
    wetLabel.setBounds (wetCol.removeFromBottom (13));
    wetCol.removeFromBottom (2);
    wetFader.setBounds (wetCol);

    auto outCol = fadersArea;
    outGainLabel.setBounds (outCol.removeFromBottom (13));
    outCol.removeFromBottom (2);
    outGainFader.setBounds (outCol);

    // Bottom SEP Knob
    int sepY = contentTopY + faderSectionH + 4;
    int sepLabelH = 12;
    int sepKnobDiam = juce::jlimit (26, 42, juce::jmin (m5Area.getWidth() - 16, sepSectionH - sepLabelH - 14));
    int sepSliderH = sepKnobDiam + 14;
    int sepSliderW = juce::jmin (m5Area.getWidth() - 10, juce::jmax (68, sepKnobDiam + 24));
    int sepPad = (sepSectionH - (sepLabelH + sepSliderH)) / 2;

    sepLabel.setBounds (m5Area.getX(), sepY + sepPad, m5Area.getWidth(), sepLabelH);
    sepSlider.setBounds (juce::Rectangle<int> (m5Area.getX(), sepY + sepPad + sepLabelH, m5Area.getWidth(), sepSliderH)
                             .withSizeKeepingCentre (sepSliderW, sepSliderH));

    if (licensePanel != nullptr)
        licensePanel->setBounds (getLocalBounds());
}

void ProVerbAudioProcessorEditor::setGuiScale (float scale)
{
    currentScaleFactor = scale;
    setScaleFactor (scale);
    scaleButton.setButtonText (juce::String (static_cast<int> (std::round (scale * 100.0f))) + "%");
    setSize (static_cast<int> (1120.0f * scale), static_cast<int> (640.0f * scale));
}

void ProVerbAudioProcessorEditor::captureSnapshot (ParameterSnapshot& target)
{
    target.values.clear();
    auto& apvts = audioProcessor.getAPVTS();
    for (auto* param : apvts.processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            target.values[ranged->getParameterID()] = ranged->convertFrom0to1 (ranged->getValue());
    }
}

void ProVerbAudioProcessorEditor::applySnapshot (const ParameterSnapshot& source)
{
    auto& apvts = audioProcessor.getAPVTS();
    for (const auto& pair : source.values)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pair.first)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (pair.second));
            p->endChangeGesture();
        }
    }
}

juce::File ProVerbAudioProcessorEditor::getUserPresetDirectory() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("ProducerToy")
                   .getChildFile ("ProVerb")
                   .getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

void ProVerbAudioProcessorEditor::saveUserToyPreset (const juce::String& presetName)
{
    if (presetName.trim().isEmpty())
        return;

    auto dir = getUserPresetDirectory();
    juce::String safeFileName = juce::File::createLegalFileName (presetName.trim());
    if (! safeFileName.endsWithIgnoreCase (".toy"))
        safeFileName += ".toy";

    auto file = dir.getChildFile (safeFileName);

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("format", "ProducerToyPreset");
    obj->setProperty ("extension", ".toy");
    obj->setProperty ("plugin", "ProVerb");
    obj->setProperty ("version", "1.0.0");
    obj->setProperty ("name", presetName.trim());
    obj->setProperty ("author", "User");
    obj->setProperty ("category", "User Presets");
    obj->setProperty ("created", juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S"));

    auto* paramsObj = new juce::DynamicObject();
    auto& apvts = audioProcessor.getAPVTS();
    for (auto* param : apvts.processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            paramsObj->setProperty (ranged->getParameterID(), ranged->convertFrom0to1 (ranged->getValue()));
    }
    obj->setProperty ("parameters", juce::var (paramsObj));

    juce::String jsonStr = juce::JSON::toString (juce::var (obj), true);
    if (file.replaceWithText (jsonStr))
    {
        rebuildPresetMenu();
        presetBox.setText ("User: " + presetName.trim(), juce::dontSendNotification);
    }
}

void ProVerbAudioProcessorEditor::loadUserToyPreset (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    juce::var parsed;
    if (juce::JSON::parse (file.loadFileAsString(), parsed).wasOk())
    {
        if (auto* obj = parsed.getDynamicObject())
        {
            auto paramsVar = obj->getProperty ("parameters");
            if (auto* paramsObj = paramsVar.getDynamicObject())
            {
                auto& apvts = audioProcessor.getAPVTS();
                for (const auto& prop : paramsObj->getProperties())
                {
                    const juce::String paramId = prop.name.toString();
                    const float val = static_cast<float> (prop.value);

                    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (paramId)))
                    {
                        p->beginChangeGesture();
                        p->setValueNotifyingHost (p->convertTo0to1 (val));
                        p->endChangeGesture();
                    }
                }

                juce::String name = obj->getProperty ("name").toString();
                if (name.isEmpty())
                    name = file.getFileNameWithoutExtension();

                lastMatchedPresetIndex = -1;
                presetBox.setSelectedId (0, juce::dontSendNotification);
                presetBox.setText ("User: " + name, juce::dontSendNotification);
            }
        }
    }
}

void ProVerbAudioProcessorEditor::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    auto* rootMenu = presetBox.getRootMenu();
    const auto& presetList = ProVerbAudioProcessor::getPresetList();

    std::map<juce::String, juce::PopupMenu> categoryMenus;
    std::vector<juce::String> categoryOrder;

    for (size_t i = 0; i < presetList.size(); ++i)
    {
        const auto& cat = presetList[i].category;
        if (categoryMenus.find (cat) == categoryMenus.end())
        {
            categoryMenus[cat] = juce::PopupMenu();
            categoryOrder.push_back (cat);
        }
        categoryMenus[cat].addItem (static_cast<int> (i + 1), presetList[i].name);
    }

    rootMenu->addItem (19995, "[ Type to Search Presets... ]");
    rootMenu->addSeparator();

    for (const auto& cat : categoryOrder)
        rootMenu->addSubMenu (cat, categoryMenus[cat]);

    rootMenu->addSeparator();

    // User .toy Presets Submenu
    juce::PopupMenu userMenu;
    auto userDir = getUserPresetDirectory();
    auto toyFiles = userDir.findChildFiles (juce::File::findFiles, false, "*.toy");

    constexpr int USER_PRESET_BASE_ID = 20000;
    if (toyFiles.isEmpty())
    {
        userMenu.addItem (19996, "No .toy presets found (Save one below)", false);
    }
    else
    {
        for (int i = 0; i < toyFiles.size(); ++i)
            userMenu.addItem (USER_PRESET_BASE_ID + i, toyFiles[i].getFileNameWithoutExtension());
    }

    userMenu.addSeparator();
    userMenu.addItem (19997, "+ Save Current State as .toy...");
    userMenu.addItem (19998, "Open Presets Folder in Explorer");
    userMenu.addItem (19999, "Import .toy File...");

    rootMenu->addSubMenu ("User Presets (.toy)", userMenu);

    presetBox.setSelectedId (audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
    lastMatchedPresetIndex = audioProcessor.getCurrentProgram();

    presetBox.onChange = [this]()
    {
        int selected = presetBox.getSelectedId();
        if (selected == 19995) // Trigger Inline Search Overlay
        {
            searchBox.setBounds (presetBox.getBounds());
            searchBox.setVisible (true);
            searchBox.toFront (true);
            searchBox.setText ("");
            searchBox.grabKeyboardFocus();
        }
        else if (selected >= 1 && selected <= static_cast<int> (ProVerbAudioProcessor::getPresetList().size()))
        {
            int prog = selected - 1;
            lastMatchedPresetIndex = prog;
            audioProcessor.setCurrentProgram (prog);
        }
        else if (selected == 19997) // Save Current State as .toy
        {
            auto* dialog = new juce::AlertWindow ("Save .toy Preset", "Enter a name for your custom ProVerb preset (.toy):", juce::AlertWindow::QuestionIcon);
            dialog->addTextEditor ("presetName", "My Custom Reverb", "Preset Name:");
            dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
            dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this, dialog](int result)
            {
                if (result == 1)
                {
                    juce::String name = dialog->getTextEditorContents ("presetName");
                    saveUserToyPreset (name);
                }
                delete dialog;
            }));
        }
        else if (selected == 19998) // Open Presets Folder
        {
            getUserPresetDirectory().startAsProcess();
        }
        else if (selected == 19999) // Import .toy
        {
            fileChooser = std::make_shared<juce::FileChooser> ("Import .toy Preset", getUserPresetDirectory(), "*.toy");
            juce::Component::SafePointer<ProVerbAudioProcessorEditor> safeThis (this);
            fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safeThis](const juce::FileChooser& fc)
                {
                    if (safeThis == nullptr) return;
                    auto file = fc.getResult();
                    if (file.existsAsFile())
                        safeThis->loadUserToyPreset (file);
                });
        }
        else if (selected >= 20000) // User .toy preset loaded
        {
            int userIdx = selected - 20000;
            auto toyFiles = getUserPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.toy");
            if (userIdx >= 0 && userIdx < toyFiles.size())
                loadUserToyPreset (toyFiles[userIdx]);
        }
    };

    audioProcessor.onProgramChange = [this](int prog)
    {
        lastMatchedPresetIndex = prog;
        presetBox.setSelectedId (prog + 1, juce::dontSendNotification);
    };
}

void ProVerbAudioProcessorEditor::performPresetSearch (const juce::String& query)
{
    const juce::String trimmed = query.trim().toLowerCase();
    if (trimmed.isEmpty())
        return;

    auto matches = ProVerbPresets::searchPresets (0, trimmed);
    if (matches.empty())
        return;

    juce::PopupMenu menu;
    int count = 0;
    const auto& factoryList = ProVerbAudioProcessor::getPresetList();
    for (int idx : matches)
    {
        if (count++ >= 14) break;
        if (idx >= 0 && idx < static_cast<int> (factoryList.size()))
        {
            const auto& p = factoryList[static_cast<size_t> (idx)];
            menu.addItem (idx + 1, p.category + " > " + p.name);
        }
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&searchBox),
        [this](int result)
        {
            if (result > 0)
            {
                int prog = result - 1;
                audioProcessor.setCurrentProgram (prog);
                lastMatchedPresetIndex = prog;
                presetBox.setSelectedId (result, juce::dontSendNotification);
                searchBox.setVisible (false);
                searchBox.clear();
            }
        });
}

void ProVerbAudioProcessorEditor::showLicensePanel()
{
    if (licensePanel == nullptr)
    {
        licensePanel = std::make_unique<ProducerToy::UI::LicensePanel> ([this]() { hideLicensePanel(); });
        addAndMakeVisible (*licensePanel);
    }
    licensePanel->refreshState();
    licensePanel->setBounds (getLocalBounds());
    licensePanel->toFront (true);
}

void ProVerbAudioProcessorEditor::hideLicensePanel()
{
    // This callback also runs after a local license removal so the audio-thread cache is refreshed.
    audioProcessor.updateLicenseStatus();

    if (! ProducerToy::Licensing::LicenseClient::getInstance().isLicenseValid())
        return;

    if (licensePanel != nullptr)
    {
        removeChildComponent (licensePanel.get());
        licensePanel.reset();
    }
}

bool ProVerbAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& file : files)
    {
        if (file.endsWithIgnoreCase (".wav")
            || file.endsWithIgnoreCase (".aif")
            || file.endsWithIgnoreCase (".aiff")
            || file.endsWithIgnoreCase (".mp3")
            || file.endsWithIgnoreCase (".flac"))
        {
            return true;
        }
    }
    return false;
}

void ProVerbAudioProcessorEditor::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    for (const auto& filePath : files)
    {
        juce::File audioFile (filePath);
        if (audioFile.existsAsFile())
        {
            wasAnalyzing = true;
            analysisWatchdogTicks = 0;
            smartAutoLearnButton.setAnalyzing (true, 0.25f, 0.5f);
            visualizer.setAnalyzing (true, 0.25f);
            modernEQ.setAnalyzing (true, 0.25f);

            audioProcessor.analyzeAudioFileAsync (audioFile);
            break;
        }
    }
}

