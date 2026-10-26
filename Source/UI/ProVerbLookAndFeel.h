#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ProVerbTokens.h"

/**
 * ProVerbLookAndFeel — Hardware Studio Aesthetic & Modern Dark Cinematic Reverb
 * Producer Toy | ProVerb Smart Algorithmic Reverb
 * 
 * Crafted with realistic audio hardware skeuomorphism and modern neon accents
 * directly faithful to the ProVerb reference design:
 * - Deep navy/graphite rotors and chassis panels
 * - Controlled neon glowing arcs & active LED indicator pips (Cyan, Blue, Violet, Magenta, Amber, Mint)
 * - Hardware capsule fader caps with illuminated center slits
 * - Multi-segment capsule toggle switches
 * - Sleek preset navigation & control buttons with hover/press transitions
 */
class ProVerbLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ProVerbLookAndFeel()
    {
        using namespace ProVerbTokens::Colors;

        setColour (juce::Slider::rotarySliderFillColourId, neonCyan);
        setColour (juce::Slider::rotarySliderOutlineColourId, border);
        setColour (juce::Slider::thumbColourId, juce::Colour (0xffffffff));
        setColour (juce::Label::textColourId, textSecondary);
        setColour (juce::ComboBox::backgroundColourId, panelElevated);
        setColour (juce::ComboBox::outlineColourId, border);
        setColour (juce::ComboBox::textColourId, textPrimary);
        setColour (juce::PopupMenu::backgroundColourId, panelElevated);
        setColour (juce::PopupMenu::textColourId, textPrimary);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff222228));
        setColour (juce::PopupMenu::highlightedTextColourId, neonCyan);
    }

    // =========================================================================
    // 1. Slider Text Box (Value Readout)
    // =========================================================================
    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (slider);
        l->setColour (juce::Label::outlineWhenEditingColourId, ProVerbTokens::Colors::neonCyan);
        l->setColour (juce::Label::textWhenEditingColourId, ProVerbTokens::Colors::textPrimary);
        l->setColour (juce::Label::backgroundWhenEditingColourId, ProVerbTokens::Colors::panelDarkSlot);
        l->setColour (juce::Label::outlineColourId, juce::Colour (0x00000000));
        l->setColour (juce::Label::textColourId, ProVerbTokens::Colors::textPrimary);
        l->setFont (ProVerbTokens::Typography::makeValueFont (11.0f));
        l->setJustificationType (juce::Justification::centred);
        return l;
    }

    // =========================================================================
    // 2. Rotary Knobs (Hardware Graphite Dial with Neon Glow Arc & Radiant Pip)
    // =========================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        using namespace ProVerbTokens::Colors;

        auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
        auto centre = area.getCentre();
        
        float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.44f;
        if (radius < 8.0f)
            return;

        auto squareBounds = juce::Rectangle<float> (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        const bool isHovered = slider.isMouseOverOrDragging();
        const bool isDown = slider.isMouseButtonDown();

        // 1. Recessed Obsidian Well (Dark socket with top inner shadow)
        g.setColour (panelDarkSlot);
        g.fillEllipse (squareBounds);

        // Socket bevel ring
        juce::ColourGradient socketGrad (border.withAlpha (0.8f), centre.x, squareBounds.getY(),
                                         border.withAlpha (0.2f), centre.x, squareBounds.getBottom(), false);
        g.setGradientFill (socketGrad);
        g.drawEllipse (squareBounds, 1.0f);

        // 2. Track Background Arc (Dark Hairline Groove)
        float arcRadius = radius - 4.5f;
        juce::Path bgArc;
        bgArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xff18181c));
        g.strokePath (bgArc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Active Luminous Value Arc with Custom Accent Color
        juce::Colour arcCol = neonCyan;
        if (slider.isColourSpecified (juce::Slider::rotarySliderFillColourId))
        {
            auto specifiedCol = slider.findColour (juce::Slider::rotarySliderFillColourId);
            if (specifiedCol != juce::Colour (0xff34363d))
                arcCol = specifiedCol;
        }

        if (sliderPosProportional > 0.001f)
        {
            juce::Path activeArc;
            activeArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                     rotaryStartAngle, toAngle, true);
            
            const float pulse = audioPulseLevel * 0.25f;
            const float hoverBoost = isHovered ? 0.12f : 0.0f;

            // Pass 1: Outer soft ambient glow
            float blurAlpha1 = juce::jlimit (0.05f, 0.25f, 0.08f + pulse * 0.10f + hoverBoost);
            g.setColour (arcCol.withAlpha (blurAlpha1));
            g.strokePath (activeArc, juce::PathStrokeType (9.0f + pulse * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Pass 2: Mid luminous bloom
            float blurAlpha2 = juce::jlimit (0.15f, 0.45f, 0.22f + pulse * 0.12f + hoverBoost);
            g.setColour (arcCol.withAlpha (blurAlpha2));
            g.strokePath (activeArc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Pass 3: Silky gradient core arc
            juce::ColourGradient arcGrad (arcCol, squareBounds.getX(), squareBounds.getY(),
                                          arcCol.brighter (0.20f), squareBounds.getRight(), squareBounds.getBottom(), false);
            g.setGradientFill (arcGrad);
            g.strokePath (activeArc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Radiant Active Beacon Indicator Pip at Current Value
            float pipX = centre.x + std::sin (toAngle) * arcRadius;
            float pipY = centre.y - std::cos (toAngle) * arcRadius;
            
            // Pip halo
            float pipHaloR = 4.8f + pulse * 1.5f;
            g.setColour (arcCol.withAlpha (juce::jlimit (0.25f, 0.60f, 0.35f + pulse * 0.15f + hoverBoost)));
            g.fillEllipse (pipX - pipHaloR, pipY - pipHaloR, pipHaloR * 2.0f, pipHaloR * 2.0f);
            
            // Pip core
            g.setColour (arcCol.brighter (0.35f));
            g.fillEllipse (pipX - 2.5f, pipY - 2.5f, 5.0f, 5.0f);

            // White jewel center
            g.setColour (juce::Colour (0xffffffff).withAlpha (0.95f));
            g.fillEllipse (pipX - 1.2f, pipY - 1.2f, 2.4f, 2.4f);
        }

        // 4. Dial Knob Body (Minimalist Floating Graphite Rotor)
        float dialRadius = radius * 0.68f;
        auto dialBounds = juce::Rectangle<float> (centre.x - dialRadius, centre.y - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f);

        // Rotor depth drop shadow
        g.setColour (juce::Colour (0x99000000));
        g.fillEllipse (dialBounds.translated (0.0f, 1.8f));

        // Precision Graphite Rotor Face (ToyTune Luxury Charcoal Rotor)
        juce::Colour rotorTop = isHovered ? juce::Colour (0xff222228) : juce::Colour (0xff18181c);
        juce::Colour rotorBot = isDown ? juce::Colour (0xff0a0a0d) : juce::Colour (0xff121215);
        juce::ColourGradient dialGrad (rotorTop, centre.x, dialBounds.getY(),
                                       rotorBot, centre.x, dialBounds.getBottom(), false);
        g.setGradientFill (dialGrad);
        g.fillEllipse (dialBounds);

        // Hairline rim with subtle hover glow
        juce::Colour rimCol = isHovered ? arcCol.withAlpha (0.6f) : border;
        g.setColour (rimCol);
        g.drawEllipse (dialBounds, 1.0f);

        // Concentric Inner Accent Ring
        float innerRingRadius = dialRadius * 0.48f;
        g.setColour (juce::Colour (0x18ffffff));
        g.drawEllipse (centre.x - innerRingRadius, centre.y - innerRingRadius,
                       innerRingRadius * 2.0f, innerRingRadius * 2.0f, 0.8f);

        // 5. Razor-Sharp Indicator Needle
        float needleStart = dialRadius * 0.24f;
        float needleEnd   = dialRadius * 0.84f;

        float sinA = std::sin (toAngle);
        float cosA = std::cos (toAngle);

        float xStart = centre.x + sinA * needleStart;
        float yStart = centre.y - cosA * needleStart;
        float xEnd   = centre.x + sinA * needleEnd;
        float yEnd   = centre.y - cosA * needleEnd;

        // Needle soft bloom
        g.setColour (arcCol.withAlpha (isHovered ? 0.45f : 0.30f));
        g.drawLine (xStart, yStart, xEnd, yEnd, 3.8f);

        // Needle core
        g.setColour (arcCol.brighter (0.4f));
        g.drawLine (xStart, yStart, xEnd, yEnd, 1.8f);

        // Needle white jewel tip
        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (xEnd - 1.3f, yEnd - 1.3f, 2.6f, 2.6f);
    }

    // =========================================================================
    // 3. Segmented Capsule Toggle (MID / SIDE vs STEREO, BYPASS, FREEZE)
    // =========================================================================
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        using namespace ProVerbTokens::Colors;
        juce::ignoreUnused (shouldDrawButtonAsDown);

        auto bounds = button.getLocalBounds().toFloat();
        bool isToggled = button.getToggleState();
        auto btnRect = bounds.reduced (1.0f, 1.0f);
        const float pillRadius = btnRect.getHeight() * 0.48f;

        // Special: Segmented Switch for MID / SIDE vs STEREO
        if (button.getButtonText().contains ("MID / SIDE") || button.getButtonText() == "MID / SIDE")
        {
            // Outer Capsule Well
            g.setColour (panelDarkSlot);
            g.fillRoundedRectangle (btnRect, pillRadius);
            g.setColour (border);
            g.drawRoundedRectangle (btnRect, pillRadius, 1.0f);

            float halfW = btnRect.getWidth() * 0.5f;
            auto leftSeg  = btnRect.withWidth (halfW);
            auto rightSeg = btnRect.withTrimmedLeft (halfW);

            auto activeSegRect = isToggled ? leftSeg.reduced (2.0f, 2.0f) : rightSeg.reduced (2.0f, 2.0f);
            
            // Active segment pill background
            juce::Colour activeTint = neonCyan;
            juce::ColourGradient segGrad (activeTint.withMultipliedBrightness (0.22f).overlaidWith (panelElevated), 0.0f, activeSegRect.getY(),
                                          activeTint.withMultipliedBrightness (0.10f).overlaidWith (panelBackground), 0.0f, activeSegRect.getBottom(), false);
            g.setGradientFill (segGrad);
            g.fillRoundedRectangle (activeSegRect, activeSegRect.getHeight() * 0.48f);

            // Glowing border on active segment
            g.setColour (activeTint.withAlpha (0.85f));
            g.drawRoundedRectangle (activeSegRect, activeSegRect.getHeight() * 0.48f, 1.0f);

            // Soft bloom
            g.setColour (activeTint.withAlpha (0.15f));
            g.drawRoundedRectangle (activeSegRect.expanded (1.0f), activeSegRect.getHeight() * 0.48f, 2.5f);

            // Typography
            g.setFont (ProVerbTokens::Typography::makeLabelFont (10.0f, true));
            
            // Left text: MID / SIDE
            g.setColour (isToggled ? textPrimary : textMuted);
            g.drawText ("MID / SIDE", leftSeg, juce::Justification::centred, true);

            // Right text: STEREO
            g.setColour (! isToggled ? textPrimary : textMuted);
            g.drawText ("STEREO", rightSeg, juce::Justification::centred, true);
            return;
        }

        // Standard Large Pill Toggle Button (BYPASS, FREEZE, etc.)
        juce::Colour activeColour = neonCyan;
        if (button.getButtonText() == "BYPASS")
            activeColour = neonCoral; // Soft warning crimson
        else if (button.isColourSpecified (juce::ToggleButton::tickColourId)
                 && button.findColour (juce::ToggleButton::tickColourId) != juce::Colour (0xffffffff))
            activeColour = button.findColour (juce::ToggleButton::tickColourId);

        // Sleek Capsule Background
        juce::Colour bgTop = isToggled ? activeColour.withMultipliedBrightness (0.25f).overlaidWith (panelElevated)
                                       : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff222228) : panelElevated);
        juce::Colour bgBot = isToggled ? activeColour.withMultipliedBrightness (0.10f).overlaidWith (panelBackground)
                                       : panelBackground;
        juce::Colour borderCol = isToggled ? activeColour.withAlpha (0.90f)
                                           : (shouldDrawButtonAsHighlighted ? border.brighter (0.25f) : border);

        juce::ColourGradient btnGrad (bgTop, 0.0f, btnRect.getY(), bgBot, 0.0f, btnRect.getBottom(), false);
        g.setGradientFill (btnGrad);
        g.fillRoundedRectangle (btnRect, pillRadius);

        // Ambient blurred glow if active
        if (isToggled)
        {
            g.setColour (activeColour.withAlpha (0.22f));
            g.drawRoundedRectangle (btnRect.expanded (1.0f), pillRadius, 3.0f);
        }

        // Hairline border
        g.setColour (borderCol);
        g.drawRoundedRectangle (btnRect, pillRadius, 1.0f);

        // Button text
        g.setFont (ProVerbTokens::Typography::makeLabelFont (10.0f, true));
        g.setColour (isToggled ? textPrimary : (shouldDrawButtonAsHighlighted ? textPrimary : textSecondary));
        g.drawText (button.getButtonText(), btnRect, juce::Justification::centred, true);
    }

    // =========================================================================
    // 4. Console Mixer Faders (INPUT, DRY, WET, OUTPUT) with Glowing Level Beam
    // =========================================================================
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        using namespace ProVerbTokens::Colors;

        if (style == juce::Slider::LinearVertical)
        {
            auto trackArea = juce::Rectangle<float> ((float) x + (float) width * 0.5f - 2.0f,
                                                     (float) y + 6.0f, 4.0f, (float) height - 12.0f);

            // 1. Recessed Fader Track Slot
            g.setColour (panelDarkSlot);
            g.fillRoundedRectangle (trackArea, 2.0f);
            g.setColour (border.withAlpha (0.7f));
            g.drawRoundedRectangle (trackArea, 2.0f, 1.0f);

            // 2. Precision Scale Ticks along track
            for (float ty = trackArea.getY() + 4.0f; ty <= trackArea.getBottom() - 4.0f; ty += 14.0f)
            {
                g.setColour (juce::Colour (0x24ffffff));
                g.drawHorizontalLine ((int) ty, trackArea.getX() - 5.0f, trackArea.getX() - 2.0f);
                g.drawHorizontalLine ((int) ty, trackArea.getRight() + 2.0f, trackArea.getRight() + 5.0f);
            }

            // 3. Luminous Active Beam below Fader Thumb
            auto activeSlot = trackArea.withTop (sliderPos);
            juce::Colour faderCol = neonCyan;
            if (slider.isColourSpecified (juce::Slider::thumbColourId))
            {
                auto specifiedCol = slider.findColour (juce::Slider::thumbColourId);
                if (specifiedCol != juce::Colour (0xffffffff))
                    faderCol = specifiedCol;
            }
            
            // Atmospheric glow pass
            g.setColour (faderCol.withAlpha (0.18f));
            g.fillRoundedRectangle (activeSlot.expanded (3.5f, 0.0f), 3.0f);
            g.setColour (faderCol.withAlpha (0.40f));
            g.fillRoundedRectangle (activeSlot.expanded (1.5f, 0.0f), 2.0f);
            
            // Core beam
            g.setColour (faderCol);
            g.fillRoundedRectangle (activeSlot, 1.8f);

            // 4. Modern Capsule Fader Cap
            float thumbW = ProVerbTokens::Metrics::faderThumbWidth;
            float thumbH = ProVerbTokens::Metrics::faderThumbHeight;
            auto thumbRect = juce::Rectangle<float> ((float) x + (float) width * 0.5f - thumbW * 0.5f,
                                                     sliderPos - thumbH * 0.5f, thumbW, thumbH);

            const bool isHovered = slider.isMouseOverOrDragging();

            // Subtle drop shadow
            g.setColour (juce::Colour (0xaa000000));
            g.fillRoundedRectangle (thumbRect.translated (0, 2.0f), 4.0f);

            // Graphite Cap Body
            juce::Colour capTop = isHovered ? juce::Colour (0xff282830) : juce::Colour (0xff1c1c22);
            juce::Colour capBot = panelBackground;
            juce::ColourGradient thumbGrad (capTop, thumbRect.getX(), thumbRect.getY(),
                                            capBot, thumbRect.getX(), thumbRect.getBottom(), false);
            g.setGradientFill (thumbGrad);
            g.fillRoundedRectangle (thumbRect, 4.0f);

            // Colored rim
            g.setColour (isHovered ? faderCol.withAlpha (0.85f) : border);
            g.drawRoundedRectangle (thumbRect, 4.0f, 1.0f);

            // Center illuminated indicator slit with glow
            g.setColour (faderCol.withAlpha (0.85f));
            g.drawHorizontalLine ((int) thumbRect.getCentreY(), thumbRect.getX() + 4.0f, thumbRect.getRight() - 4.0f);
            
            g.setColour (juce::Colour (0xffffffff));
            g.fillRect (thumbRect.getX() + 6.0f, thumbRect.getCentreY() - 0.75f, thumbW - 12.0f, 1.5f);
        }
        else
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }

    // =========================================================================
    // 5. Preset ComboBox Dropdown Display (Capsule Shape)
    // =========================================================================
    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                       juce::ComboBox& box) override
    {
        using namespace ProVerbTokens::Colors;

        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
        const float cornerR = bounds.getHeight() * 0.48f;

        // Recessed Dark Glass Capsule
        juce::Colour bgTop = isButtonDown ? panelDarkSlot : (box.isMouseOver() ? juce::Colour (0xff222228) : panelElevated);
        juce::Colour bgBot = panelBackground;
        juce::Colour borderCol = box.isMouseOver() ? neonCyan.withAlpha (0.80f) : border;

        juce::ColourGradient grad (bgTop, 0.0f, bounds.getY(), bgBot, 0.0f, bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, cornerR);

        g.setColour (borderCol);
        g.drawRoundedRectangle (bounds, cornerR, 1.0f);

        // Minimalist Chevron Arrow
        auto arrowZone = bounds.removeFromRight (24.0f);
        juce::Path arrow;
        float ax = arrowZone.getCentreX();
        float ay = arrowZone.getCentreY();
        arrow.startNewSubPath (ax - 3.5f, ay - 1.5f);
        arrow.lineTo (ax, ay + 2.0f);
        arrow.lineTo (ax + 3.5f, ay - 1.5f);

        g.setColour (box.isMouseOver() ? textPrimary : textSecondary);
        g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // =========================================================================
    // 6. Push Buttons (A / B, Settings, Menu, Navigation, Heart)
    // =========================================================================
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return ProVerbTokens::Typography::makeLabelFont (juce::jlimit (9.5f, 11.5f, (float) buttonHeight * 0.38f), true);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        using namespace ProVerbTokens::Colors;

        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const float cornerR = bounds.getHeight() * 0.48f;

        if (button.getComponentID() == "heartPresetBtn")
        {
            const bool isFav = button.getToggleState();
            juce::Colour bgTop = shouldDrawButtonAsDown ? panelDarkSlot
                               : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff222228) : panelElevated);
            juce::Colour bgBot = panelBackground;
            juce::ColourGradient grad (bgTop, 0.0f, bounds.getY(), bgBot, 0.0f, bounds.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bounds, cornerR);

            juce::Colour borderCol;
            if (isFav)
                borderCol = shouldDrawButtonAsHighlighted ? neonMagenta : neonMagenta.withAlpha (0.85f);
            else if (shouldDrawButtonAsHighlighted)
                borderCol = neonMagenta.withAlpha (0.6f);
            else
                borderCol = border;

            g.setColour (borderCol);
            g.drawRoundedRectangle (bounds, cornerR, 1.0f);
            return;
        }

        if (button.getComponentID() == "prevPresetBtn" || button.getComponentID() == "nextPresetBtn")
        {
            juce::Colour bgTop = shouldDrawButtonAsDown ? panelDarkSlot
                               : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff222228) : panelElevated);
            juce::Colour bgBot = panelBackground;
            juce::ColourGradient grad (bgTop, 0.0f, bounds.getY(), bgBot, 0.0f, bounds.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bounds, cornerR);

            juce::Colour borderCol = shouldDrawButtonAsHighlighted ? neonCyan.withAlpha (0.8f) : border;
            g.setColour (borderCol);
            g.drawRoundedRectangle (bounds, cornerR, 1.0f);
            return;
        }

        juce::Colour bg = backgroundColour;
        if (shouldDrawButtonAsDown)
            bg = bg.darker (0.25f);
        else if (shouldDrawButtonAsHighlighted)
            bg = bg.brighter (0.15f);

        juce::ColourGradient grad (bg.brighter (0.05f), bounds.getX(), bounds.getY(),
                                   bg.darker (0.15f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, cornerR);

        juce::Colour borderCol = shouldDrawButtonAsHighlighted ? neonCyan.withAlpha (0.80f) : border;
        g.setColour (borderCol);
        g.drawRoundedRectangle (bounds, cornerR, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        using namespace ProVerbTokens::Colors;

        if (button.getComponentID() == "heartPresetBtn")
        {
            auto bounds = button.getLocalBounds().toFloat();
            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY();
            const bool isFav = button.getToggleState();

            // Studio-grade vector heart
            const float w = 15.0f;
            const float h = 13.5f;
            const float l = cx - w * 0.5f;
            const float t = cy - h * 0.5f - 0.5f;

            juce::Path heart;
            heart.startNewSubPath (l + w * 0.5f, t + h);
            heart.cubicTo (l + w * 0.05f, t + h * 0.65f,
                           l,             t + h * 0.35f,
                           l,             t + h * 0.22f);
            heart.cubicTo (l,             t + h * 0.02f,
                           l + w * 0.22f, t,
                           l + w * 0.40f, t);
            heart.cubicTo (l + w * 0.48f, t,
                           l + w * 0.50f, t + h * 0.12f,
                           l + w * 0.50f, t + h * 0.20f);
            heart.cubicTo (l + w * 0.50f, t + h * 0.12f,
                           l + w * 0.52f, t,
                           l + w * 0.60f, t);
            heart.cubicTo (l + w * 0.78f, t,
                           l + w,         t + h * 0.02f,
                           l + w,         t + h * 0.22f);
            heart.cubicTo (l + w,         t + h * 0.35f,
                           l + w * 0.95f, t + h * 0.65f,
                           l + w * 0.5f,  t + h);
            heart.closeSubPath();

            if (isFav)
            {
                g.setColour (neonMagenta.withAlpha (0.25f));
                g.strokePath (heart, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (shouldDrawButtonAsHighlighted ? neonMagenta.brighter (0.15f) : neonMagenta);
                g.fillPath (heart);
            }
            else
            {
                juce::Colour strokeCol = shouldDrawButtonAsHighlighted ? neonMagenta : textSecondary;
                g.setColour (strokeCol);
                g.strokePath (heart, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            return;
        }

        if (button.getComponentID() == "prevPresetBtn")
        {
            auto bounds = button.getLocalBounds().toFloat();
            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY();
            juce::Path chev;
            chev.startNewSubPath (cx + 2.0f, cy - 4.5f);
            chev.lineTo (cx - 2.5f, cy);
            chev.lineTo (cx + 2.0f, cy + 4.5f);
            g.setColour (shouldDrawButtonAsHighlighted ? textPrimary : textSecondary);
            g.strokePath (chev, juce::PathStrokeType (1.75f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            return;
        }

        if (button.getComponentID() == "nextPresetBtn")
        {
            auto bounds = button.getLocalBounds().toFloat();
            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY();
            juce::Path chev;
            chev.startNewSubPath (cx - 2.0f, cy - 4.5f);
            chev.lineTo (cx + 2.5f, cy);
            chev.lineTo (cx - 2.0f, cy + 4.5f);
            g.setColour (shouldDrawButtonAsHighlighted ? textPrimary : textSecondary);
            g.strokePath (chev, juce::PathStrokeType (1.75f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            return;
        }

        auto font = getTextButtonFont (button, button.getHeight());
        g.setFont (font);
        g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                               : juce::TextButton::textColourOffId)
                            .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));

        g.drawText (button.getButtonText(), button.getLocalBounds(),
                    juce::Justification::centred, false);
    }

    void setAudioPulseLevel (float level) noexcept
    {
        audioPulseLevel = juce::jlimit (0.0f, 1.0f, level);
    }

private:
    float audioPulseLevel { 0.0f };
};
