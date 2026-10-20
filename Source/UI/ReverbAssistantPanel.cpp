#include "ReverbAssistantPanel.h"
#include "../PluginProcessor.h"
#include <cmath>

namespace ProducerToy::UI
{
    ReverbAssistantPanel::ReverbAssistantPanel (ProVerbAudioProcessor& proc,
                                                std::function<void()> onCloseCallback,
                                                std::function<void(const juce::String&, const juce::String&)> onPresetAppliedCallback)
        : processor (proc),
          onClose (std::move (onCloseCallback)),
          onPresetApplied (std::move (onPresetAppliedCallback))
    {
        setOpaque (false);
        setAlwaysOnTop (true);
        startTimerHz (30); // 30 FPS smooth animations & audio detection
    }

    ReverbAssistantPanel::~ReverbAssistantPanel()
    {
        stopTimer();
    }

    void ReverbAssistantPanel::startAssistantFlow()
    {
        resetFlow();
        setVisible (true);
        toFront (true);
    }

    void ReverbAssistantPanel::resetFlow()
    {
        currentStage = Stage::Setup;
        analysisFinished = false;
        workingTicks = 0;
        simulatedProgress = 0.0f;
        autoCloseCountdown = -1;
        detectedInstrument.clear();
        appliedPresetName.clear();
        repaint();
    }

    juce::Rectangle<float> ReverbAssistantPanel::getCardBounds() const
    {
        const float cardW = juce::jlimit (520.0f, 640.0f, (float) getWidth() * 0.72f);
        const float cardH = juce::jlimit (420.0f, 490.0f, (float) getHeight() * 0.82f);
        return juce::Rectangle<float> (((float) getWidth() - cardW) * 0.5f,
                                       ((float) getHeight() - cardH) * 0.5f,
                                       cardW, cardH);
    }

    juce::Rectangle<float> ReverbAssistantPanel::getCloseButtonBounds() const
    {
        auto card = getCardBounds();
        return juce::Rectangle<float> (card.getRight() - 38.0f, card.getY() + 14.0f, 26.0f, 26.0f);
    }

    juce::Rectangle<float> ReverbAssistantPanel::getNextButtonBounds() const
    {
        auto card = getCardBounds();
        return juce::Rectangle<float> (card.getRight() - 120.0f, card.getBottom() - 48.0f, 100.0f, 32.0f);
    }

    juce::Rectangle<float> ReverbAssistantPanel::getStyleSliderBounds (juce::Rectangle<float> contentArea) const
    {
        const float sliderW = contentArea.getWidth() * 0.70f;
        const float sliderH = 14.0f;
        return juce::Rectangle<float> (contentArea.getCentreX() - sliderW * 0.5f,
                                       contentArea.getY() + 42.0f,
                                       sliderW, sliderH);
    }

    juce::Rectangle<float> ReverbAssistantPanel::getSizeTrackBounds (juce::Rectangle<float> contentArea) const
    {
        const float secY = contentArea.getY() + 115.0f;
        const float leftW = contentArea.getWidth() * 0.62f;
        return juce::Rectangle<float> (contentArea.getX() + 24.0f, secY + 22.0f, leftW - 36.0f, 16.0f);
    }

    juce::Rectangle<float> ReverbAssistantPanel::getDryWetKnobBounds (juce::Rectangle<float> contentArea) const
    {
        const float secY = contentArea.getY() + 105.0f;
        const float knobX = contentArea.getRight() - 110.0f;
        return juce::Rectangle<float> (knobX, secY, 70.0f, 70.0f);
    }

    juce::Rectangle<float> ReverbAssistantPanel::getToneTileBounds (juce::Rectangle<float> contentArea, int index) const
    {
        const float secY = contentArea.getY() + 215.0f;
        const float totalW = contentArea.getWidth() - 48.0f;
        const float gap = 10.0f;
        const float tileW = (totalW - (gap * 3.0f)) / 4.0f;
        const float tileH = 82.0f;
        return juce::Rectangle<float> (contentArea.getX() + 24.0f + (float) index * (tileW + gap),
                                       secY + 22.0f,
                                       tileW, tileH);
    }

    void ReverbAssistantPanel::timerCallback()
    {
        animPhase += 0.08f;
        if (animPhase > juce::MathConstants<float>::twoPi * 10.0f)
            animPhase -= juce::MathConstants<float>::twoPi * 10.0f;

        const float inRms = processor.getRmsInput();
        pulseLevel += (inRms - pulseLevel) * 0.25f;

        if (currentStage == Stage::Listening)
        {
            // Auto advance when audio plays in DAW
            if (inRms > 0.005f)
            {
                advanceToListening();
            }
        }
        else if (currentStage == Stage::Working)
        {
            workingTicks++;
            simulatedProgress = juce::jlimit (0.0f, 1.0f, (float) workingTicks / 75.0f); // ~2.5s total

            // Check if backend analyzer has completed
            if (! processor.isAutoLearning() && workingTicks > 50 && ! analysisFinished)
            {
                analysisFinished = true;
                currentStage = Stage::Done;

                juce::String inst = processor.getDetectedInstrument();
                juce::String preset = processor.getDetectedPresetName();

                if (inst.isEmpty() || inst == "NO AUDIO")
                    inst = "STUDIO ACOUSTIC";
                if (preset.isEmpty())
                    preset = "Warm Studio Vocal Hall";

                detectedInstrument = inst;
                appliedPresetName = preset;
                confidenceScore = juce::jlimit (0.75f, 0.98f, processor.getDetectedSourceConfidence() > 0.1f ? processor.getDetectedSourceConfidence() : 0.88f);

                // Apply customized parameters according to user choices
                applyCustomizedPreset (inst, preset);

                // Start auto-close timer (countdown 90 frames = ~3 seconds)
                autoCloseCountdown = 90;
            }
        }
        else if (currentStage == Stage::Done)
        {
            if (autoCloseCountdown > 0)
            {
                autoCloseCountdown--;
                if (autoCloseCountdown == 0)
                {
                    if (onClose)
                        onClose();
                }
            }
        }

        repaint();
    }

    void ReverbAssistantPanel::paint (juce::Graphics& g)
    {
        // 1. Dark frosted backdrop dimming out main UI
        g.setColour (juce::Colour (0xdd08090c));
        g.fillRect (getLocalBounds());

        auto card = getCardBounds();

        // 2. Card Shadow & Drop Glow
        g.setColour (juce::Colour (0x44000000));
        g.fillRoundedRectangle (card.expanded (4.0f), 14.0f);

        // 3. Card Base: Pure Deep Obsidian Charcoal (NO BLUE TINT)
        juce::ColourGradient cardGrad (juce::Colour (0xff12141a), card.getX(), card.getY(),
                                       juce::Colour (0xff0a0b0e), card.getX(), card.getBottom(), false);
        g.setGradientFill (cardGrad);
        g.fillRoundedRectangle (card, 12.0f);

        // 4. Subtle Obsidian Cyber Border
        g.setColour (juce::Colour (0xff222530));
        g.drawRoundedRectangle (card, 12.0f, 1.0f);

        // Card Top Edge Bevel
        g.setColour (juce::Colour (0x15ffffff));
        g.drawHorizontalLine ((int) card.getY() + 1, card.getX() + 12.0f, card.getRight() - 12.0f);

        // 5. Header Area
        auto headerArea = card.removeFromTop (50.0f);
        
        // Header Title
        g.setFont (juce::FontOptions ("Segoe UI", 16.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xfff1f5f9));

        if (currentStage == Stage::Setup)
        {
            // Title: ◎ Play audio & begin
            g.drawText (juce::CharPointer_UTF8 ("\xe2\x97\x8e  Play audio & begin"),
                        headerArea.reduced (24.0f, 0.0f),
                        juce::Justification::centredLeft, true);
        }
        else if (currentStage == Stage::Listening)
        {
            g.drawText ("Waiting for you to play audio...",
                        headerArea.reduced (24.0f, 0.0f),
                        juce::Justification::centredLeft, true);
        }
        else // Working or Done
        {
            g.drawText (currentStage == Stage::Done ? "Reverb Assistant Complete" : "Assistant is working...",
                        headerArea.reduced (24.0f, 0.0f),
                        juce::Justification::centredLeft, true);
        }

        // Close Button (✕) in top right
        auto closeBtnRect = getCloseButtonBounds();
        g.setColour (juce::Colour (0xff181a22));
        g.fillEllipse (closeBtnRect);
        g.setColour (juce::Colour (0xff2c303f));
        g.drawEllipse (closeBtnRect, 1.0f);

        g.setFont (juce::FontOptions ("Segoe UI", 13.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xff94a3b8));
        g.drawText (juce::CharPointer_UTF8 ("\xc3\x97"), closeBtnRect, juce::Justification::centred, false);

        // 6. Draw Screen Content by Stage
        auto contentArea = card.withTrimmedBottom (56.0f);

        if (currentStage == Stage::Setup)
        {
            drawSetupScreen (g, contentArea);
        }
        else if (currentStage == Stage::Listening)
        {
            drawListeningScreen (g, contentArea);
        }
        else // Working / Done
        {
            drawWorkingScreen (g, contentArea);
        }

        // 7. Footer: Pagination Dots & Navigation Action
        auto footerArea = juce::Rectangle<float> (card.getX(), card.getBottom() - 54.0f, card.getWidth(), 54.0f);
        auto dotArea = juce::Rectangle<float> (footerArea.getCentreX() - 40.0f, footerArea.getY() + 18.0f, 80.0f, 16.0f);

        int dotIdx = 0;
        if (currentStage == Stage::Listening) dotIdx = 1;
        else if (currentStage == Stage::Working || currentStage == Stage::Done) dotIdx = 2;
        drawDots (g, dotArea, dotIdx);

        // Next / Accept Button
        auto nextBtn = getNextButtonBounds();
        if (currentStage == Stage::Setup)
        {
            g.setColour (juce::Colour (0xff1e222d));
            g.fillRoundedRectangle (nextBtn, 16.0f);
            g.setColour (juce::Colour (0xffa855f7).withAlpha (0.8f));
            g.drawRoundedRectangle (nextBtn, 16.0f, 1.0f);

            g.setFont (juce::FontOptions ("Segoe UI", 12.0f, juce::Font::bold));
            g.setColour (juce::Colours::white);
            g.drawText (juce::CharPointer_UTF8 ("Next  \xe2\x9e\x94"), nextBtn, juce::Justification::centred, false);
        }
        else if (currentStage == Stage::Listening)
        {
            g.setColour (juce::Colour (0xff1e222d));
            g.fillRoundedRectangle (nextBtn, 16.0f);
            g.setColour (juce::Colour (0xff06b6d4).withAlpha (0.8f));
            g.drawRoundedRectangle (nextBtn, 16.0f, 1.0f);

            g.setFont (juce::FontOptions ("Segoe UI", 11.5f, juce::Font::bold));
            g.setColour (juce::Colours::white);
            g.drawText ("Analyze Now", nextBtn, juce::Justification::centred, false);
        }
        else if (currentStage == Stage::Done)
        {
            auto acceptBtn = nextBtn.withWidth (115.0f).withX (nextBtn.getRight() - 115.0f);
            g.setColour (juce::Colour (0xff06b6d4));
            g.fillRoundedRectangle (acceptBtn, 16.0f);

            g.setFont (juce::FontOptions ("Segoe UI", 12.0f, juce::Font::bold));
            g.setColour (juce::Colour (0xff05070a));
            g.drawText (juce::CharPointer_UTF8 ("Accept \xe2\x9e\x94"), acceptBtn, juce::Justification::centred, false);
        }
    }

    void ReverbAssistantPanel::drawSetupScreen (juce::Graphics& g, juce::Rectangle<float> contentArea)
    {
        // ---------------- Section 1: Style ----------------
        g.setFont (juce::FontOptions ("Segoe UI", 12.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xff94a3b8));
        g.drawText ("Style", contentArea.withY (contentArea.getY() + 16.0f).withHeight (20.0f),
                    juce::Justification::centred, false);

        auto styleBounds = getStyleSliderBounds (contentArea);

        // Style Gradient Track (Realistic Cyan -> Dramatic Violet/Rose)
        juce::ColourGradient styleGrad (juce::Colour (0xff06b6d4), styleBounds.getX(), styleBounds.getCentreY(),
                                        juce::Colour (0xfff43f5e), styleBounds.getRight(), styleBounds.getCentreY(), false);
        styleGrad.addColour (0.5, juce::Colour (0xffa855f7));
        g.setGradientFill (styleGrad);
        g.fillRoundedRectangle (styleBounds, 7.0f);

        // Style Thumb (Circular Puck)
        float thumbX = styleBounds.getX() + styleValue * styleBounds.getWidth();
        float thumbY = styleBounds.getCentreY();
        juce::Rectangle<float> thumbRect (thumbX - 11.0f, thumbY - 11.0f, 22.0f, 22.0f);

        g.setColour (juce::Colour (0x66000000));
        g.fillEllipse (thumbRect.expanded (2.0f));
        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (thumbRect);
        g.setColour (juce::Colour (0xff0a0b0e));
        g.fillEllipse (thumbRect.reduced (5.5f));

        // Style Labels
        g.setFont (juce::FontOptions ("Segoe UI", 11.0f, juce::Font::plain));
        g.setColour (juce::Colour (0xff64748b));
        g.drawText ("Realistic", styleBounds.getX() - 10.0f, styleBounds.getBottom() + 4.0f, 70.0f, 16.0f, juce::Justification::centredLeft, false);
        g.drawText ("Dramatic", styleBounds.getRight() - 60.0f, styleBounds.getBottom() + 4.0f, 70.0f, 16.0f, juce::Justification::centredRight, false);

        // ---------------- Section 2: Size & Dry/Wet ----------------
        auto sizeTrack = getSizeTrackBounds (contentArea);
        
        // Section Header: Size
        g.setFont (juce::FontOptions ("Segoe UI", 12.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xff94a3b8));
        g.drawText ("Size", sizeTrack.withY (sizeTrack.getY() - 20.0f).withHeight (18.0f),
                    juce::Justification::centred, false);

        // Size Track Background Line
        g.setColour (juce::Colour (0xff202430));
        g.fillRoundedRectangle (sizeTrack.withHeight (4.0f).withY (sizeTrack.getCentreY() - 2.0f), 2.0f);

        // Active Segment Line
        float activeW = ((float) sizeStep / 4.0f) * sizeTrack.getWidth();
        g.setColour (juce::Colour (0xffa855f7));
        g.fillRoundedRectangle (sizeTrack.getX(), sizeTrack.getCentreY() - 2.0f, activeW, 4.0f, 2.0f);

        // 5 Notches: XS, S, M, L, XL
        const char* sizeLabels[5] = { "XS", "S", "M", "L", "XL" };
        for (int i = 0; i < 5; ++i)
        {
            float nx = sizeTrack.getX() + ((float) i / 4.0f) * sizeTrack.getWidth();
            float ny = sizeTrack.getCentreY();
            bool isSel = (i == sizeStep);

            if (isSel)
            {
                g.setColour (juce::Colour (0xffc084fc).withAlpha (0.4f));
                g.fillEllipse (nx - 9.0f, ny - 9.0f, 18.0f, 18.0f);
                g.setColour (juce::Colours::white);
                g.fillEllipse (nx - 6.0f, ny - 6.0f, 12.0f, 12.0f);
                g.setColour (juce::Colour (0xff0a0b0e));
                g.fillEllipse (nx - 2.5f, ny - 2.5f, 5.0f, 5.0f);
            }
            else
            {
                g.setColour (i < sizeStep ? juce::Colour (0xffa855f7) : juce::Colour (0xff3b4050));
                g.fillEllipse (nx - 3.5f, ny - 3.5f, 7.0f, 7.0f);
            }

            g.setFont (juce::FontOptions ("Segoe UI", 10.5f, isSel ? juce::Font::bold : juce::Font::plain));
            g.setColour (isSel ? juce::Colours::white : juce::Colour (0xff64748b));
            g.drawText (sizeLabels[i], nx - 16.0f, ny + 10.0f, 32.0f, 16.0f, juce::Justification::centred, false);
        }

        // Vertical divider separating Size and Dry/Wet
        float divX = sizeTrack.getRight() + 20.0f;
        g.setColour (juce::Colour (0xff202430));
        g.drawVerticalLine ((int) divX, contentArea.getY() + 115.0f, contentArea.getY() + 185.0f);

        // Dry/Wet Circular Rotary Gauge
        auto dwRect = getDryWetKnobBounds (contentArea);
        auto dwCentre = dwRect.getCentre();
        float dwRadius = 24.0f;

        // Background Arc
        juce::Path bgArc;
        bgArc.addCentredArc (dwCentre.x, dwCentre.y - 4.0f, dwRadius, dwRadius, 0.0f,
                             -juce::MathConstants<float>::pi * 0.75f,
                             juce::MathConstants<float>::pi * 0.75f, true);
        g.setColour (juce::Colour (0xff202430));
        g.strokePath (bgArc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active Arc
        float dwPct = juce::jlimit (0.05f, 1.0f, dryWetPercent / 100.0f);
        float endAngle = -juce::MathConstants<float>::pi * 0.75f + dwPct * (juce::MathConstants<float>::pi * 1.5f);
        juce::Path actArc;
        actArc.addCentredArc (dwCentre.x, dwCentre.y - 4.0f, dwRadius, dwRadius, 0.0f,
                              -juce::MathConstants<float>::pi * 0.75f, endAngle, true);
        g.setColour (juce::Colour (0xff06b6d4));
        g.strokePath (actArc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Percentage Text in center
        g.setFont (juce::FontOptions ("Segoe UI", 12.0f, juce::Font::bold));
        g.setColour (juce::Colours::white);
        g.drawText (juce::String ((int) std::round (dryWetPercent)),
                    dwRect.withTrimmedBottom (16.0f), juce::Justification::centred, false);

        // Dry/Wet Label below
        g.setFont (juce::FontOptions ("Segoe UI", 10.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xff94a3b8));
        g.drawText ("Dry/Wet", dwRect.withY (dwRect.getBottom() - 14.0f).withHeight (16.0f),
                    juce::Justification::centred, false);

        // ---------------- Section 3: Tone Tiles ----------------
        g.setFont (juce::FontOptions ("Segoe UI", 12.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xff94a3b8));
        g.drawText ("Tone", contentArea.withY (contentArea.getY() + 195.0f).withHeight (18.0f),
                    juce::Justification::centred, false);

        const char* toneNames[4] = { "Clean", "Dark", "Bright", "Airy" };
        for (int i = 0; i < 4; ++i)
        {
            auto tileRect = getToneTileBounds (contentArea, i);
            bool isSel = (static_cast<int> (currentTone) == i);

            if (isSel)
            {
                juce::ColourGradient tileGrad (juce::Colour (0xff251a36), tileRect.getX(), tileRect.getY(),
                                               juce::Colour (0xff14121a), tileRect.getX(), tileRect.getBottom(), false);
                g.setGradientFill (tileGrad);
                g.fillRoundedRectangle (tileRect, 8.0f);
                g.setColour (juce::Colour (0xffa855f7));
                g.drawRoundedRectangle (tileRect, 8.0f, 1.5f);
            }
            else
            {
                g.setColour (juce::Colour (0xff12141a));
                g.fillRoundedRectangle (tileRect, 8.0f);
                g.setColour (juce::Colour (0xff202430));
                g.drawRoundedRectangle (tileRect, 8.0f, 1.0f);
            }

            // Draw Tone Icon
            auto iconArea = tileRect.removeFromTop (48.0f);
            auto iconCentre = iconArea.getCentre();
            g.setColour (isSel ? juce::Colour (0xffc084fc) : juce::Colour (0xff64748b));

            if (i == 0) // Clean: Sparkles
            {
                g.fillEllipse (iconCentre.x - 2.0f, iconCentre.y - 8.0f, 4.0f, 4.0f);
                g.fillEllipse (iconCentre.x + 6.0f, iconCentre.y + 2.0f, 3.0f, 3.0f);
                g.fillEllipse (iconCentre.x - 7.0f, iconCentre.y + 4.0f, 3.0f, 3.0f);
                g.drawLine (iconCentre.x, iconCentre.y - 12.0f, iconCentre.x, iconCentre.y - 4.0f, 1.5f);
                g.drawLine (iconCentre.x - 4.0f, iconCentre.y - 8.0f, iconCentre.x + 4.0f, iconCentre.y - 8.0f, 1.5f);
            }
            else if (i == 1) // Dark: Cloud / Rain
            {
                g.drawHorizontalLine ((int) iconCentre.y - 4, iconCentre.x - 10.0f, iconCentre.x + 10.0f);
                g.fillRoundedRectangle (iconCentre.x - 12.0f, iconCentre.y - 8.0f, 24.0f, 10.0f, 5.0f);
                g.drawLine (iconCentre.x - 6.0f, iconCentre.y + 6.0f, iconCentre.x - 9.0f, iconCentre.y + 11.0f, 1.5f);
                g.drawLine (iconCentre.x, iconCentre.y + 6.0f, iconCentre.x - 3.0f, iconCentre.y + 11.0f, 1.5f);
                g.drawLine (iconCentre.x + 6.0f, iconCentre.y + 6.0f, iconCentre.x + 3.0f, iconCentre.y + 11.0f, 1.5f);
            }
            else if (i == 2) // Bright: Sunburst
            {
                g.drawEllipse (iconCentre.x - 5.0f, iconCentre.y - 5.0f, 10.0f, 10.0f, 1.5f);
                for (int ray = 0; ray < 8; ++ray)
                {
                    float angle = (float) ray * (juce::MathConstants<float>::twoPi / 8.0f);
                    float x1 = iconCentre.x + std::cos (angle) * 8.0f;
                    float y1 = iconCentre.y + std::sin (angle) * 8.0f;
                    float x2 = iconCentre.x + std::cos (angle) * 12.0f;
                    float y2 = iconCentre.y + std::sin (angle) * 12.0f;
                    g.drawLine (x1, y1, x2, y2, 1.5f);
                }
            }
            else // Airy: Wind Breeze Lines
            {
                juce::Path breeze1;
                breeze1.startNewSubPath (iconCentre.x - 10.0f, iconCentre.y - 6.0f);
                breeze1.quadraticTo (iconCentre.x + 6.0f, iconCentre.y - 6.0f, iconCentre.x + 6.0f, iconCentre.y - 10.0f);
                g.strokePath (breeze1, juce::PathStrokeType (1.5f));

                juce::Path breeze2;
                breeze2.startNewSubPath (iconCentre.x - 12.0f, iconCentre.y);
                breeze2.quadraticTo (iconCentre.x + 10.0f, iconCentre.y, iconCentre.x + 10.0f, iconCentre.y - 4.0f);
                g.strokePath (breeze2, juce::PathStrokeType (1.5f));

                juce::Path breeze3;
                breeze3.startNewSubPath (iconCentre.x - 8.0f, iconCentre.y + 6.0f);
                breeze3.quadraticTo (iconCentre.x + 4.0f, iconCentre.y + 6.0f, iconCentre.x + 4.0f, iconCentre.y + 2.0f);
                g.strokePath (breeze3, juce::PathStrokeType (1.5f));
            }

            // Tile Label
            g.setFont (juce::FontOptions ("Segoe UI", 11.5f, isSel ? juce::Font::bold : juce::Font::plain));
            g.setColour (isSel ? juce::Colours::white : juce::Colour (0xff94a3b8));
            g.drawText (toneNames[i], tileRect.withHeight (22.0f), juce::Justification::centred, false);
        }
    }

    void ReverbAssistantPanel::drawListeningScreen (juce::Graphics& g, juce::Rectangle<float> contentArea)
    {
        auto centre = contentArea.getCentre();

        // Animated Concentric Acoustic Breathing Waves
        for (int ring = 1; ring <= 3; ++ring)
        {
            float ringPhase = std::fmod (animPhase + (float) ring * 1.5f, 4.0f);
            float ringR = 40.0f + ringPhase * 35.0f;
            float alpha = juce::jlimit (0.0f, 0.4f, (1.0f - (ringPhase / 4.0f)) * 0.4f + pulseLevel * 0.3f);

            g.setColour (juce::Colour (0xff06b6d4).withAlpha (alpha));
            g.drawEllipse (centre.x - ringR, centre.y - 20.0f - ringR, ringR * 2.0f, ringR * 2.0f, 1.5f);
        }

        // Center Minimalist Musical Note with Clock Face
        float noteX = centre.x;
        float noteY = centre.y - 20.0f;

        // Clock head of the musical note
        g.setColour (juce::Colour (0xff141720));
        g.fillEllipse (noteX - 22.0f, noteY - 10.0f, 32.0f, 32.0f);
        g.setColour (juce::Colour (0xff38bdf8));
        g.drawEllipse (noteX - 22.0f, noteY - 10.0f, 32.0f, 32.0f, 2.0f);

        // Clock Hands inside note head
        float clockHourAngle = animPhase * 0.5f;
        float clockMinAngle  = animPhase * 2.0f;
        g.setColour (juce::Colour (0xfff1f5f9));
        g.drawLine (noteX - 6.0f, noteY + 6.0f,
                    noteX - 6.0f + std::sin (clockHourAngle) * 7.0f,
                    noteY + 6.0f - std::cos (clockHourAngle) * 7.0f, 2.0f);
        g.drawLine (noteX - 6.0f, noteY + 6.0f,
                    noteX - 6.0f + std::sin (clockMinAngle) * 10.0f,
                    noteY + 6.0f - std::cos (clockMinAngle) * 10.0f, 1.5f);

        // Musical Note Stem and Flag
        g.setColour (juce::Colour (0xff38bdf8));
        g.drawLine (noteX + 10.0f, noteY + 6.0f, noteX + 10.0f, noteY - 36.0f, 2.5f);
        
        juce::Path flag;
        flag.startNewSubPath (noteX + 10.0f, noteY - 36.0f);
        flag.lineTo (noteX + 26.0f, noteY - 24.0f);
        flag.lineTo (noteX + 10.0f, noteY - 18.0f);
        flag.closeSubPath();
        g.setColour (juce::Colour (0xff38bdf8));
        g.fillPath (flag);

        // Floating Small Notes around the center
        float fNotes[4][2] = { { -65.0f, -40.0f }, { 65.0f, -30.0f }, { -55.0f, 40.0f }, { 60.0f, 35.0f } };
        for (int fn = 0; fn < 4; ++fn)
        {
            float fx = centre.x + fNotes[fn][0];
            float fy = centre.y - 20.0f + fNotes[fn][1] + std::sin (animPhase + (float) fn) * 6.0f;
            g.setFont (juce::FontOptions ("Segoe UI", 16.0f, juce::Font::bold));
            g.setColour (juce::Colour (0xff64748b).withAlpha (0.7f + 0.3f * std::sin (animPhase * 2.0f + (float) fn)));
            g.drawText (fn % 2 == 0 ? juce::CharPointer_UTF8 ("\xe2\x99\xaa") : juce::CharPointer_UTF8 ("\xe2\x99\xab"),
                        fx - 10.0f, fy - 10.0f, 20.0f, 20.0f, juce::Justification::centred, false);
        }

        // Subtitle instructions
        g.setFont (juce::FontOptions ("Segoe UI", 13.0f, juce::Font::plain));
        g.setColour (juce::Colour (0xff94a3b8));
        g.drawText ("Play audio in your DAW or press Analyze Now below...",
                    contentArea.withY (contentArea.getBottom() - 48.0f).withHeight (24.0f),
                    juce::Justification::centred, false);
    }

    void ReverbAssistantPanel::drawWorkingScreen (juce::Graphics& g, juce::Rectangle<float> contentArea)
    {
        float startY = contentArea.getY() + 24.0f;
        float rowH = 68.0f;
        float w = contentArea.getWidth() - 80.0f;
        float x = contentArea.getX() + 40.0f;

        // Stage 1: Auto Cut
        bool isCutDone = simulatedProgress >= 0.33f;
        bool isCutActive = simulatedProgress < 0.33f;

        // Header + Icon: ✂ Auto Cut
        g.setFont (juce::FontOptions ("Segoe UI", 13.5f, juce::Font::bold));
        g.setColour (isCutDone || isCutActive ? juce::Colours::white : juce::Colour (0xff64748b));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x9c\x82  Auto Cut"), juce::Rectangle<float> (x, startY, w, 20.0f).toNearestInt(), juce::Justification::centredLeft, false);

        // Status Line
        g.setFont (juce::FontOptions ("Segoe UI", 11.5f, juce::Font::plain));
        g.setColour (isCutDone ? juce::Colour (0xff38bdf8) : (isCutActive ? juce::Colour (0xffcbd5e1) : juce::Colour (0xff64748b)));
        juce::String cutText = isCutDone ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93 Cleaning up input signal by creating custom Pre EQ curve"))
                                         : (isCutActive ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9f\xb3 Cleaning up input signal by creating custom Pre EQ curve..."))
                                                        : juce::String ("Pre EQ curve synthesis pending"));
        g.drawText (cutText, juce::Rectangle<float> (x + 8.0f, startY + 22.0f, w, 18.0f).toNearestInt(), juce::Justification::centredLeft, false);

        // Divider 1
        float div1Y = startY + rowH;
        g.setColour (juce::Colour (0xff1f232e));
        g.drawHorizontalLine ((int) div1Y, x, x + w);

        // Stage 2: Unmask
        bool isUnmaskDone = simulatedProgress >= 0.68f;
        bool isUnmaskActive = simulatedProgress >= 0.33f && simulatedProgress < 0.68f;

        float unmaskY = div1Y + 12.0f;
        g.setFont (juce::FontOptions ("Segoe UI", 13.5f, juce::Font::bold));
        g.setColour (isUnmaskDone || isUnmaskActive ? juce::Colours::white : juce::Colour (0xff64748b));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x96\xa6  Unmask"), juce::Rectangle<float> (x, unmaskY, w, 20.0f).toNearestInt(), juce::Justification::centredLeft, false);

        g.setFont (juce::FontOptions ("Segoe UI", 11.5f, juce::Font::plain));
        g.setColour (isUnmaskDone ? juce::Colour (0xffa855f7) : (isUnmaskActive ? juce::Colour (0xffcbd5e1) : juce::Colour (0xff64748b)));
        juce::String unmaskText = isUnmaskDone ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93 Comparing wet and dry signal to create custom Post EQ curve"))
                                               : (isUnmaskActive ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9f\xb3 Comparing wet and dry signal to create custom Post EQ curve..."))
                                                                 : juce::String ("Comparing wet and dry signal"));
        g.drawText (unmaskText, juce::Rectangle<float> (x + 8.0f, unmaskY + 22.0f, w, 18.0f).toNearestInt(), juce::Justification::centredLeft, false);

        // Divider 2
        float div2Y = unmaskY + rowH;
        g.setColour (juce::Colour (0xff1f232e));
        g.drawHorizontalLine ((int) div2Y, x, x + w);

        // Stage 3: Space Tuning
        bool isSpaceDone = simulatedProgress >= 0.98f;
        bool isSpaceActive = simulatedProgress >= 0.68f;

        float spaceY = div2Y + 12.0f;
        g.setFont (juce::FontOptions ("Segoe UI", 13.5f, juce::Font::bold));
        g.setColour (isSpaceDone || isSpaceActive ? juce::Colours::white : juce::Colour (0xff64748b));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x9c\xa6  Space Tuning"), juce::Rectangle<float> (x, spaceY, w, 20.0f).toNearestInt(), juce::Justification::centredLeft, false);

        g.setFont (juce::FontOptions ("Segoe UI", 11.5f, juce::Font::plain));
        g.setColour (isSpaceDone ? juce::Colour (0xff34d399) : (isSpaceActive ? juce::Colour (0xffcbd5e1) : juce::Colour (0xff64748b)));
        juce::String spaceText = isSpaceDone ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93 Synthesizing custom 3D acoustic reflection & tail matrix"))
                                             : (isSpaceActive ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9f\xb3 Synthesizing custom 3D acoustic reflection & tail matrix..."))
                                                              : juce::String ("Reflection matrix calculation"));
        g.drawText (spaceText, juce::Rectangle<float> (x + 8.0f, spaceY + 22.0f, w, 18.0f).toNearestInt(), juce::Justification::centredLeft, false);

        // Result Pill Badge (when finished)
        if (currentStage == Stage::Done && detectedInstrument.isNotEmpty())
        {
            auto badgeArea = juce::Rectangle<float> (x, spaceY + 48.0f, w, 32.0f);
            g.setColour (juce::Colour (0xff151a24));
            g.fillRoundedRectangle (badgeArea, 6.0f);
            g.setColour (juce::Colour (0xff38bdf8).withAlpha (0.6f));
            g.drawRoundedRectangle (badgeArea, 6.0f, 1.0f);

            juce::String badgeMsg = juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x94 DETECTED: ")) + detectedInstrument
                                    + " (" + juce::String ((int) std::round (confidenceScore * 100.0f)) + "% MATCH)  ->  " + appliedPresetName;
            g.setFont (juce::FontOptions ("Segoe UI", 11.0f, juce::Font::bold));
            g.setColour (juce::Colour (0xff38bdf8));
            g.drawText (badgeMsg, badgeArea.toNearestInt(), juce::Justification::centred, false);
        }
        else
        {
            // Progress Bar
            auto barRect = juce::Rectangle<float> (x, spaceY + 54.0f, w, 6.0f);
            g.setColour (juce::Colour (0xff181a22));
            g.fillRoundedRectangle (barRect, 3.0f);

            float fillW = barRect.getWidth() * simulatedProgress;
            juce::ColourGradient barGrad (juce::Colour (0xff06b6d4), barRect.getX(), barRect.getY(),
                                          juce::Colour (0xffa855f7), barRect.getX() + fillW, barRect.getY(), false);
            g.setGradientFill (barGrad);
            g.fillRoundedRectangle (barRect.getX(), barRect.getY(), fillW, barRect.getHeight(), 3.0f);
        }
    }

    void ReverbAssistantPanel::drawDots (juce::Graphics& g, juce::Rectangle<float> dotArea, int activeIndex)
    {
        const float dotSize = 6.0f;
        const float gap = 12.0f;
        float startX = dotArea.getCentreX() - ((3.0f * dotSize + 2.0f * gap) * 0.5f);
        float y = dotArea.getCentreY() - (dotSize * 0.5f);

        for (int i = 0; i < 3; ++i)
        {
            float dx = startX + (float) i * (dotSize + gap);
            bool isAct = (i == activeIndex);
            g.setColour (isAct ? juce::Colours::white : juce::Colour (0xff333846));
            g.fillEllipse (dx, y, dotSize, dotSize);
        }
    }

    void ReverbAssistantPanel::advanceToListening()
    {
        currentStage = Stage::Listening;
        repaint();
    }

    void ReverbAssistantPanel::triggerAnalysis()
    {
        currentStage = Stage::Working;
        workingTicks = 0;
        simulatedProgress = 0.0f;
        analysisFinished = false;
        processor.triggerAutoLearn();
        repaint();
    }

    void ReverbAssistantPanel::applyCustomizedPreset (const juce::String& instrument, const juce::String& presetName)
    {
        auto& apvts = processor.getAPVTS();

        auto setParam = [&apvts](const juce::String& paramId, float value)
        {
            if (auto* p = apvts.getParameter (paramId))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (value));
                p->endChangeGesture();
            }
        };

        // 1. Size Step mapping
        float targetSize = 50.0f;
        float targetDecay = 1.8f;
        float targetPredelay = 25.0f;

        switch (sizeStep)
        {
            case 0: targetSize = 25.0f; targetDecay = 0.70f; targetPredelay = 10.0f; break; // XS
            case 1: targetSize = 40.0f; targetDecay = 1.20f; targetPredelay = 16.0f; break; // S
            case 2: targetSize = 62.0f; targetDecay = 1.90f; targetPredelay = 28.0f; break; // M
            case 3: targetSize = 82.0f; targetDecay = 3.20f; targetPredelay = 45.0f; break; // L
            case 4: targetSize = 96.0f; targetDecay = 5.50f; targetPredelay = 65.0f; break; // XL
            default: break;
        }

        // 2. Style Modulation (0.0 Realistic -> 1.0 Dramatic)
        float sMod = styleValue - 0.5f; // -0.5 to +0.5
        targetDecay = juce::jlimit (0.3f, 15.0f, targetDecay * (1.0f + sMod * 0.7f));
        float targetShimmer = juce::jlimit (0.0f, 60.0f, 5.0f + sMod * 35.0f);
        float targetDiffusion = juce::jlimit (30.0f, 100.0f, 75.0f + sMod * 25.0f);
        float targetWidth = juce::jlimit (75.0f, 160.0f, 100.0f + sMod * 40.0f);

        // 3. Tone Modulation
        float targetLowCut = 160.0f;
        float targetHighCut = 11000.0f;
        float targetDamping = 40.0f;

        switch (currentTone)
        {
            case ToneOption::Clean:
                targetLowCut = 180.0f; targetHighCut = 10500.0f; targetDamping = 40.0f;
                break;
            case ToneOption::Dark:
                targetLowCut = 80.0f;  targetHighCut = 4200.0f;  targetDamping = 72.0f;
                break;
            case ToneOption::Bright:
                targetLowCut = 260.0f; targetHighCut = 16000.0f; targetDamping = 18.0f;
                break;
            case ToneOption::Airy:
                targetLowCut = 340.0f; targetHighCut = 20000.0f; targetDamping = 20.0f;
                targetShimmer = juce::jmax (targetShimmer, 18.0f);
                break;
        }

        // Apply safely to APVTS
        setParam ("size",         targetSize);
        setParam ("decaySeconds", targetDecay);
        setParam ("predelayMs",   targetPredelay);
        setParam ("diffusion",    targetDiffusion);
        setParam ("damping",      targetDamping);
        setParam ("lowCut",       targetLowCut);
        setParam ("highCut",      targetHighCut);
        setParam ("shimmer",      targetShimmer);
        setParam ("width",        targetWidth);
        setParam ("wet",          dryWetPercent);
        setParam ("dry",          100.0f);

        if (onPresetApplied)
            onPresetApplied (instrument, presetName);
    }

    void ReverbAssistantPanel::resized()
    {
    }

    void ReverbAssistantPanel::mouseDown (const juce::MouseEvent& e)
    {
        auto card = getCardBounds();
        if (! card.contains (e.position))
        {
            // Click outside card closes overlay
            if (onClose)
                onClose();
            return;
        }

        if (getCloseButtonBounds().contains (e.position))
        {
            if (onClose)
                onClose();
            return;
        }

        auto nextBtn = getNextButtonBounds();
        if (nextBtn.contains (e.position))
        {
            if (currentStage == Stage::Setup)
            {
                advanceToListening();
                return;
            }
            else if (currentStage == Stage::Listening)
            {
                triggerAnalysis();
                return;
            }
            else if (currentStage == Stage::Done)
            {
                if (onClose)
                    onClose();
                return;
            }
        }

        auto contentArea = card.removeFromTop (card.getHeight() - 56.0f);

        if (currentStage == Stage::Setup)
        {
            // Style Slider Hit
            auto styleBounds = getStyleSliderBounds (contentArea).expanded (12.0f, 10.0f);
            if (styleBounds.contains (e.position))
            {
                isDraggingStyle = true;
                auto actualBar = getStyleSliderBounds (contentArea);
                styleValue = juce::jlimit (0.0f, 1.0f, (e.position.x - actualBar.getX()) / actualBar.getWidth());
                repaint();
                return;
            }

            // Size Notches Hit
            auto sizeTrack = getSizeTrackBounds (contentArea).expanded (10.0f, 14.0f);
            if (sizeTrack.contains (e.position))
            {
                auto actualTrack = getSizeTrackBounds (contentArea);
                float stepF = (e.position.x - actualTrack.getX()) / actualTrack.getWidth() * 4.0f;
                sizeStep = juce::jlimit (0, 4, (int) std::round (stepF));
                repaint();
                return;
            }

            // Dry/Wet Knob Hit
            auto dwRect = getDryWetKnobBounds (contentArea);
            if (dwRect.contains (e.position))
            {
                isDraggingDryWet = true;
                dragStartY = e.position.y;
                dragStartDryWet = dryWetPercent;
                return;
            }

            // Tone Tiles Hit
            for (int i = 0; i < 4; ++i)
            {
                auto tileRect = getToneTileBounds (contentArea, i);
                if (tileRect.contains (e.position))
                {
                    currentTone = static_cast<ToneOption> (i);
                    repaint();
                    return;
                }
            }
        }
    }

    void ReverbAssistantPanel::mouseDrag (const juce::MouseEvent& e)
    {
        auto card = getCardBounds();
        auto contentArea = card.removeFromTop (card.getHeight() - 56.0f);

        if (isDraggingStyle)
        {
            auto actualBar = getStyleSliderBounds (contentArea);
            styleValue = juce::jlimit (0.0f, 1.0f, (e.position.x - actualBar.getX()) / actualBar.getWidth());
            repaint();
        }
        else if (isDraggingDryWet)
        {
            float deltaY = dragStartY - e.position.y;
            dryWetPercent = juce::jlimit (5.0f, 100.0f, dragStartDryWet + deltaY * 0.6f);
            repaint();
        }
    }

    void ReverbAssistantPanel::mouseUp (const juce::MouseEvent&)
    {
        isDraggingStyle = false;
        isDraggingDryWet = false;
    }

    void ReverbAssistantPanel::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        auto card = getCardBounds();
        auto contentArea = card.removeFromTop (card.getHeight() - 56.0f);
        if (getDryWetKnobBounds (contentArea).contains (e.position))
        {
            dryWetPercent = juce::jlimit (5.0f, 100.0f, dryWetPercent + wheel.deltaY * 20.0f);
            repaint();
        }
    }
}
