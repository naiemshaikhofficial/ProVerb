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
        const float cornerR = 5.0f;

        // 1. Futuristic Studio Chassis Background & Cyber Border (ToyTune Black Palette)
        juce::Colour bgTop (0xff141418);
        juce::Colour bgBot (0xff0a0b0e);
        juce::Colour borderCol (0xff1e2027);

        if (isAnalyzing)
        {
            bgTop = juce::Colour (0xff101116);
            bgBot = juce::Colour (0xff07080a);
            borderCol = juce::Colour (0xff00f0ff).withAlpha (isMouseOver ? 0.95f : 0.70f);
        }
        else if (hasStickyResult)
        {
            bgTop = juce::Colour (0xff14151a);
            bgBot = juce::Colour (0xff0d0e12);
            borderCol = juce::Colour (0xff242429);
        }
        else if (isMouseOver)
        {
            bgTop = juce::Colour (0xff1c1d24);
            bgBot = juce::Colour (0xff101115);
            borderCol = juce::Colour (0xff00f0ff).withAlpha (0.75f);
        }

        juce::ColourGradient grad (bgTop, 0.0f, bounds.getY(),
                                   bgBot, 0.0f, bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds.reduced (0.5f), cornerR);

        g.setColour (borderCol);
        g.drawRoundedRectangle (bounds.reduced (0.5f), cornerR, 1.0f);

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
        float reW = juce::jlimit (54.0f, 68.0f, bounds.getWidth() * 0.48f);
        return juce::Rectangle<float> (bounds.getRight() - reW - 3.5f, bounds.getY() + 3.0f, reW, bounds.getHeight() - 6.0f);
    }

    void paintIdleState (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        // High-Tech AI Neural Assistant Icon (Left)
        float iconSize = 12.0f;
        float iconX = bounds.getX() + 10.0f;
        float iconY = bounds.getCentreY() - iconSize * 0.5f;

        g.setColour (isMouseOver ? juce::Colour (0xff00f0ff) : juce::Colour (0xff38bdf8));
        
        // 3 mini vertical EQ spectrum bars as acoustic emblem
        float barW = 2.0f;
        float barGap = 2.0f;
        g.fillRoundedRectangle (iconX, iconY + 3.0f, barW, 6.0f, 1.0f);
        g.fillRoundedRectangle (iconX + barW + barGap, iconY, barW, 12.0f, 1.0f);
        g.fillRoundedRectangle (iconX + (barW + barGap) * 2.0f, iconY + 2.0f, barW, 8.0f, 1.0f);

        // Center / Main Typography
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.setColour (isMouseOver ? juce::Colour (0xffffffff) : juce::Colour (0xffe2e8f0));
        g.drawText ("SMART REVERB AI", bounds.withTrimmedLeft (24.0f).withTrimmedRight (6.0f),
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
        auto reanalyzeRect = getReanalyzeBounds();

        // 1. Detected Instrument Badge (Left)
        // High-tech studio graphite pill with ice cyan accent
        float badgeX = bounds.getX() + 4.0f;
        float badgeY = bounds.getY() + 3.0f;
        float badgeH = bounds.getHeight() - 6.0f;

        juce::String cleanInst = detectedInstrument;
        if (cleanInst.containsIgnoreCase ("Guitar")) cleanInst = "GUITAR";
        else if (cleanInst.containsIgnoreCase ("Vocal")) cleanInst = "VOCALS";
        else if (cleanInst.containsIgnoreCase ("Drum") || cleanInst.containsIgnoreCase ("Perc")) cleanInst = "DRUMS";
        else if (cleanInst.containsIgnoreCase ("Bass")) cleanInst = "BASS";
        else if (cleanInst.containsIgnoreCase ("Keys") || cleanInst.containsIgnoreCase ("Piano")) cleanInst = "KEYS";
        else if (cleanInst.containsIgnoreCase ("Synth") || cleanInst.containsIgnoreCase ("Pad")) cleanInst = "SYNTH";
        else if (cleanInst.containsIgnoreCase ("Flute") || cleanInst.containsIgnoreCase ("Wind")) cleanInst = "FLUTE";
        else if (cleanInst.containsIgnoreCase ("String")) cleanInst = "STRINGS";
        else cleanInst = cleanInst.upToFirstOccurrenceOf (" ", false, false);
        if (cleanInst.isEmpty()) cleanInst = "AI LEARN";

#if defined(DEBUG) || defined(_DEBUG) || JUCE_DEBUG || PROVERB_DEV_MODE
        const bool devMode = true;
#else
        const bool devMode = juce::SystemStats::getEnvironmentVariable ("PROVERB_DEV", juce::String()).isNotEmpty();
#endif
        if (! devMode)
            cleanInst = "OPTIMIZED";

        juce::Font badgeFont (juce::FontOptions (9.5f, juce::Font::bold));
        float availBadgeW = std::max (30.0f, reanalyzeRect.getX() - badgeX - 4.0f);
        float badgeW = std::min (availBadgeW, static_cast<float> (badgeFont.getStringWidth (cleanInst)) + 18.0f);

        auto badgeRect = juce::Rectangle<float> (badgeX, badgeY, badgeW, badgeH);
        g.setColour (juce::Colour (0xff1c1c22));
        g.fillRoundedRectangle (badgeRect, 4.0f);

        // High-tech Cyan/Ice accent dot & border
        g.setColour (juce::Colour (0xff38bdf8).withAlpha (0.45f));
        g.drawRoundedRectangle (badgeRect, 4.0f, 1.0f);

        // Glowing Cyan Dot
        g.setColour (juce::Colour (0xff38bdf8));
        g.fillEllipse (badgeX + 5.0f, badgeY + badgeH * 0.5f - 2.5f, 5.0f, 5.0f);

        // Instrument Name
        g.setFont (badgeFont);
        g.setColour (juce::Colour (0xffffffff));
        g.drawText (cleanInst, badgeRect.withTrimmedLeft (13.0f).withTrimmedRight (3.0f),
                    juce::Justification::centredLeft, true);

        // 2. Dedicated Re-Scan Button (Right)
        juce::Colour reBtnBg = isOverReanalyze ? juce::Colour (0xff2a2a32) : juce::Colour (0xff16171b);
        juce::Colour reBtnBorder = isOverReanalyze ? juce::Colour (0xff40404c) : juce::Colour (0xff242429);

        g.setColour (reBtnBg);
        g.fillRoundedRectangle (reanalyzeRect, 4.0f);
        g.setColour (reBtnBorder);
        g.drawRoundedRectangle (reanalyzeRect, 4.0f, 1.0f);

        // Refresh icon & "RE-SCAN"
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.setColour (isOverReanalyze ? juce::Colour (0xffffffff) : juce::Colour (0xff94a3b8));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x9f\xb3 RE-SCAN"), reanalyzeRect, juce::Justification::centred, true);
    }
};
