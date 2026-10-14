#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include <algorithm>

/**
 * SmartAutoLearnButton
 * 
 * High-end studio aesthetic custom JUCE component for ProVerb's AI Acoustic Analyzer.
 * - Idle: Sleek dark studio pill with AI neural pulse icon & "AUTO ANALYZE" typography.
 * - Analyzing: Live audio-reactive waveform reacting dynamically to DAW playback,
 *   with a cyan laser scanline and real-time progress percentage.
 * - Learned (STICKY): Permanently displays the detected instrument (VOCALS, DRUMS, GUITAR, etc.),
 *   the applied industry preset, and a dedicated "RE-ANALYZE" button so users can re-trigger anytime.
 * - Theme: 100% professional studio charcoal (#16181f) with platinum & ice-cyan accents (NO green).
 */
class SmartAutoLearnButton : public juce::Component
{
public:
    SmartAutoLearnButton()
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void()> onClick;
    std::function<void()> onReanalyzeClick;

    void setAnalyzing (bool analyzing, float newProgress = 0.0f, float newAudioRms = 0.0f)
    {
        isAnalyzing = analyzing;
        if (analyzing)
        {
            hasStickyResult = false;
            detectedInstrument.clear();
            detectedPreset.clear();
        }
        progress = juce::jlimit (0.0f, 1.0f, newProgress);
        targetAudioLevel = juce::jlimit (0.0f, 1.0f, newAudioRms);
        repaint();
    }

    void setLearnedResult (const juce::String& instrument, const juce::String& presetName, bool isSuccess = true)
    {
        isAnalyzing = false;
        hasStickyResult = true;
        resultSuccess = isSuccess;
        detectedInstrument = instrument.toUpperCase();
        detectedPreset = presetName;
        repaint();
    }

    void resetToIdle()
    {
        isAnalyzing = false;
        hasStickyResult = false;
        detectedInstrument = {};
        detectedPreset = {};
        repaint();
    }

    void advanceAnimation (float delta)
    {
        // Advance wave oscillation phase
        wavePhase += delta * 5.0f;
        if (wavePhase > juce::MathConstants<float>::twoPi * 10.0f)
            wavePhase -= juce::MathConstants<float>::twoPi * 10.0f;

        // Smooth audio envelope follower (fast attack, smooth decay)
        if (targetAudioLevel > currentAudioLevel)
            currentAudioLevel += (targetAudioLevel - currentAudioLevel) * 0.4f;
        else
            currentAudioLevel += (targetAudioLevel - currentAudioLevel) * 0.1f;

        if (isAnalyzing || isMouseOver || isOverReanalyze)
            repaint();
    }

    bool isCurrentlyAnalyzing() const noexcept { return isAnalyzing; }
    bool hasResult() const noexcept { return hasStickyResult; }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (hasStickyResult && !isAnalyzing)
        {
            bool overReanalyze = getReanalyzeBounds().contains (e.position);
            if (overReanalyze != isOverReanalyze)
            {
                isOverReanalyze = overReanalyze;
                repaint();
            }
        }
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        isMouseOver = true;
        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        isMouseOver = false;
        isOverReanalyze = false;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (hasStickyResult && !isAnalyzing)
        {
            if (getReanalyzeBounds().contains (e.position))
            {
                if (onReanalyzeClick)
                    onReanalyzeClick();
                else if (onClick)
                    onClick();
                return;
            }
        }

        if (onClick)
            onClick();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const float cornerR = 7.0f;

        // 1. Studio Chassis Background & Border
        juce::Colour bgTop (0xff1c1e26);
        juce::Colour bgBot (0xff121319);
        juce::Colour borderCol (0xff333742);

        if (isAnalyzing)
        {
            bgTop = juce::Colour (0xff121622);
            bgBot = juce::Colour (0xff0a0d16);
            borderCol = juce::Colour (0xff38bdf8).withAlpha (isMouseOver ? 0.9f : 0.65f);
        }
        else if (hasStickyResult)
        {
            bgTop = juce::Colour (0xff171922);
            bgBot = juce::Colour (0xff101117);
            borderCol = juce::Colour (0xff3e4352);
        }
        else if (isMouseOver)
        {
            bgTop = juce::Colour (0xff242732);
            bgBot = juce::Colour (0xff191a24);
            borderCol = juce::Colour (0xff4f5567);
        }

        juce::ColourGradient grad (bgTop, 0.0f, bounds.getY(),
                                   bgBot, 0.0f, bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds.reduced (0.5f), cornerR);

        g.setColour (borderCol);
        g.drawRoundedRectangle (bounds.reduced (0.5f), cornerR, 1.2f);

        // =====================================================================
        // STATE 1: ANALYZING (DYNAMIC LIVE AUDIO WAVEFORM)
        // =====================================================================
        if (isAnalyzing)
        {
            paintAnalyzingState (g, bounds);
            return;
        }

        // =====================================================================
        // STATE 2: LEARNED RESULT (STICKY INSTRUMENT BADGE & RE-ANALYZE)
        // =====================================================================
        if (hasStickyResult)
        {
            paintLearnedState (g, bounds);
            return;
        }

        // =====================================================================
        // STATE 3: IDLE STATE
        // =====================================================================
        paintIdleState (g, bounds);
    }

private:
    bool isAnalyzing { false };
    bool hasStickyResult { false };
    bool resultSuccess { false };
    bool isMouseOver { false };
    bool isOverReanalyze { false };

    float progress { 0.0f };
    float currentAudioLevel { 0.0f };
    float targetAudioLevel { 0.0f };
    float wavePhase { 0.0f };

    juce::String detectedInstrument;
    juce::String detectedPreset;

    juce::Rectangle<float> getReanalyzeBounds() const
    {
        auto bounds = getLocalBounds().toFloat();
        return juce::Rectangle<float> (bounds.getRight() - 92.0f, bounds.getY() + 3.5f, 86.0f, bounds.getHeight() - 7.0f);
    }

    void paintIdleState (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        // High-Tech AI Neural Assistant Icon (Left)
        float iconSize = 14.0f;
        float iconX = bounds.getX() + 14.0f;
        float iconY = bounds.getCentreY() - iconSize * 0.5f;

        g.setColour (isMouseOver ? juce::Colour (0xff38bdf8) : juce::Colour (0xff94a3b8));
        
        // 3 mini vertical EQ spectrum bars as acoustic emblem
        float barW = 2.5f;
        float barGap = 2.5f;
        g.fillRoundedRectangle (iconX, iconY + 4.0f, barW, 6.0f, 1.0f);
        g.fillRoundedRectangle (iconX + barW + barGap, iconY + 1.0f, barW, 12.0f, 1.0f);
        g.fillRoundedRectangle (iconX + (barW + barGap) * 2.0f, iconY + 3.0f, barW, 8.0f, 1.0f);

        // Center / Main Typography - like Ozone's "Master Assistant"
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.setColour (isMouseOver ? juce::Colour (0xffffffff) : juce::Colour (0xfff1f5f9));
        g.drawText ("SMART REVERB AI", bounds.withTrimmedLeft (34.0f).withTrimmedRight (14.0f),
                    juce::Justification::centred, false);
    }

    void paintAnalyzingState (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        // 1. Progress fill underlay
        float progW = bounds.getWidth() * progress;
        if (progW > 0.0f)
        {
            juce::ColourGradient progGrad (juce::Colour (0x2438bdf8), bounds.getX(), bounds.getY(),
                                           juce::Colour (0x0638bdf8), bounds.getX() + progW, bounds.getY(), false);
            g.setGradientFill (progGrad);
            g.fillRoundedRectangle (bounds.getX(), bounds.getY(), progW, bounds.getHeight(), 8.0f);
        }

        // 2. Dynamic Audio Waveform
        const int numPoints = 42;
        juce::Path wavePath;
        float cy = bounds.getCentreY();
        float startX = bounds.getX() + 14.0f;
        float endX = bounds.getRight() - 75.0f;
        float w = endX - startX;

        float amp = 3.5f + currentAudioLevel * 10.0f;

        for (int i = 0; i < numPoints; ++i)
        {
            float t = (float) i / (float) (numPoints - 1);
            float px = startX + t * w;
            float freq1 = std::sin (t * 14.0f + wavePhase);
            float freq2 = std::sin (t * 26.0f - wavePhase * 1.3f) * 0.5f;
            float window = std::sin (t * juce::MathConstants<float>::pi);
            float py = cy + (freq1 + freq2) * amp * window;

            if (i == 0)
                wavePath.startNewSubPath (px, py);
            else
                wavePath.lineTo (px, py);
        }

        // Cyan Waveform Glow & Stroke
        g.setColour (juce::Colour (0x4438bdf8));
        g.strokePath (wavePath, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff38bdf8));
        g.strokePath (wavePath, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Sweeping Laser Scan Line
        float scanX = startX + progress * w;
        g.setColour (juce::Colour (0xffffffff));
        g.drawVerticalLine ((int) scanX, bounds.getY() + 3.0f, bounds.getBottom() - 3.0f);

        // 4. Progress percentage readout on the right
        int pct = juce::jlimit (0, 100, (int) (progress * 100.0f));
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.setColour (juce::Colour (0xfff1f5f9));
        g.drawText (juce::String (pct) + "%", (int) (bounds.getRight() - 65.0f), (int) bounds.getY(), 55, (int) bounds.getHeight(),
                    juce::Justification::centredRight, true);
    }

    void paintLearnedState (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        // 1. Detected Instrument Badge (Left)
        // High-tech studio graphite pill with ice cyan accent
        float badgeX = bounds.getX() + 6.0f;
        float badgeY = bounds.getY() + 3.5f;
        float badgeH = bounds.getHeight() - 7.0f;
        
        juce::Font badgeFont (juce::FontOptions (10.0f, juce::Font::bold));
        float badgeW = 90.0f;

        auto badgeRect = juce::Rectangle<float> (badgeX, badgeY, badgeW, badgeH);
        g.setColour (juce::Colour (0xff20242f));
        g.fillRoundedRectangle (badgeRect, 5.0f);

        // High-tech Cyan/Ice accent dot & border
        g.setColour (juce::Colour (0xff38bdf8).withAlpha (0.45f));
        g.drawRoundedRectangle (badgeRect, 5.0f, 1.0f);

        // Glowing Cyan Dot
        g.setColour (juce::Colour (0xff38bdf8));
        g.fillEllipse (badgeX + 7.0f, badgeY + badgeH * 0.5f - 2.5f, 5.0f, 5.0f);

        // Instrument Name
        g.setFont (badgeFont);
        g.setColour (juce::Colour (0xffffffff));
        g.drawText (detectedInstrument, badgeRect.withTrimmedLeft (17.0f).withTrimmedRight (4.0f),
                    juce::Justification::centredLeft, true);

        // 2. Preset / Status Description (Center)
        float textX = badgeRect.getRight() + 10.0f;
        float textW = bounds.getRight() - 100.0f - textX;
        if (textW > 20.0f)
        {
            g.setFont (juce::FontOptions (10.0f, juce::Font::plain));
            g.setColour (juce::Colour (0xffcbd5e1));

            juce::String disp = detectedPreset;
            if (disp.containsChar ('(') && disp.containsChar (')'))
                disp = disp.fromFirstOccurrenceOf ("(", false, false).upToLastOccurrenceOf (")", false, false);

            disp = disp.replace ("•", " - ")
                       .replace ("\xe2\x80\xa2", " - ")
                       .replace ("â—†", " - ")
                       .replace ("â–¡¢", " - ")
                       .replace ("â", "")
                       .replace ("¢", "")
                       .replace ("--", "-")
                       .replace ("  ", " ")
                       .trim();

            g.drawText (disp, (int) textX, (int) bounds.getY(), (int) textW, (int) bounds.getHeight(),
                        juce::Justification::centredLeft, true);
        }

        // 3. Dedicated Re-Scan Button (Right)
        auto reanalyzeRect = getReanalyzeBounds();
        juce::Colour reBtnBg = isOverReanalyze ? juce::Colour (0xff2d3240) : juce::Colour (0xff1e212b);
        juce::Colour reBtnBorder = isOverReanalyze ? juce::Colour (0xff525c6e) : juce::Colour (0xff333846);

        g.setColour (reBtnBg);
        g.fillRoundedRectangle (reanalyzeRect, 5.0f);
        g.setColour (reBtnBorder);
        g.drawRoundedRectangle (reanalyzeRect, 5.0f, 1.0f);

        // Refresh icon & "RE-SCAN"
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.setColour (isOverReanalyze ? juce::Colour (0xffffffff) : juce::Colour (0xff94a3b8));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x9f\xb3 RE-SCAN"), reanalyzeRect, juce::Justification::centred, true);
    }
};
