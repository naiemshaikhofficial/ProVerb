#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * ProVerbLookAndFeel — Hardware 3D Studio Aesthetic
 * ProducerToy — ProVerb Smart Algorithmic Reverb
 * 
 * Crafted with realistic audio hardware skeuomorphism inspired by FL Studio / Fruity Love Philter,
 * while faithfully preserving ProducerToy's signature theme:
 * - Slate Charcoal #34363d knobs & faders
 * - High-tech Ice-Cyan (#38bdf8 / #00f0ff) glowing indicator arcs & active LED accents
 * - Tactile hardware drop shadows, knurled bevels, recessed sockets, and illuminated switches
 */
class ProVerbLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ProVerbLookAndFeel()
    {
        // Custom Theme: #34363d (RGB: 52, 54, 61, HSL: 227, 8, 22)
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff34363d));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff252528));
        setColour (juce::Slider::thumbColourId, juce::Colour (0xffffffff));
        setColour (juce::Label::textColourId, juce::Colour (0xff9e9ea6));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff181920));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2d303a));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xffffffff));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141417));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffffffff));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2d313d));
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (slider);
        l->setColour (juce::Label::outlineWhenEditingColourId, juce::Colour (0xff38bdf8));
        l->setColour (juce::Label::textWhenEditingColourId, juce::Colour (0xffffffff));
        l->setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xff101115));
        l->setColour (juce::Label::outlineColourId, juce::Colour (0x00000000));
        l->setColour (juce::Label::textColourId, juce::Colour (0xffcbd5e1));
        l->setFont (juce::FontOptions (10.5f, juce::Font::bold));
        return l;
    }

    // 1. Futuristic Minimalist Rotary Knobs (Obsidian Core with Glowing Luminous Neon Arc & Radiant Pip)
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
        auto centre = area.getCentre();
        
        float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.44f;
        if (radius < 10.0f)
            return;

        auto squareBounds = juce::Rectangle<float> (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // 1. Recessed Obsidian Circular Bed (Ultra-Clean Deep Black Well)
        g.setColour (juce::Colour (0xff08090c));
        g.fillEllipse (squareBounds);

        // Hairline cyber rim (subtle bevel light catch on top, shadow below)
        juce::ColourGradient socketGrad (juce::Colour (0xff1f222a), centre.x, squareBounds.getY(),
                                         juce::Colour (0xff0a0b0e), centre.x, squareBounds.getBottom(), false);
        g.setGradientFill (socketGrad);
        g.drawEllipse (squareBounds, 1.0f);

        // 2. Track Background Arc (Minimalist Dark Hairline Slot)
        float arcRadius = radius - 4.2f;
        juce::Path bgArc;
        bgArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xff15161c));
        g.strokePath (bgArc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Active Luminous Value Arc (Soft Blurred Pastel Glow with Custom Accent)
        juce::Colour arcCol = juce::Colour (0xff38bdf8);
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
            
            // Soft Diffused Ambient Bloom (Atmospheric multi-layer blur, non-harsh)
            const float pulse = audioPulseLevel * 0.35f;
            
            // Pass 1: Outer atmospheric soft blur
            float blurAlpha1 = juce::jlimit (0.06f, 0.20f, 0.09f + pulse * 0.12f);
            g.setColour (arcCol.withAlpha (blurAlpha1));
            g.strokePath (activeArc, juce::PathStrokeType (9.5f + pulse * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Pass 2: Mid soft bloom
            float blurAlpha2 = juce::jlimit (0.15f, 0.38f, 0.22f + pulse * 0.16f);
            g.setColour (arcCol.withAlpha (blurAlpha2));
            g.strokePath (activeArc, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Pass 3: Core silky pastel arc
            juce::ColourGradient arcGrad (arcCol, squareBounds.getX(), squareBounds.getY(),
                                          arcCol.brighter (0.18f), squareBounds.getRight(), squareBounds.getBottom(), false);
            g.setGradientFill (arcGrad);
            g.strokePath (activeArc, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Radiant Active Beacon Indicator Pip at current value
            float pipX = centre.x + std::sin (toAngle) * arcRadius;
            float pipY = centre.y - std::cos (toAngle) * arcRadius;
            
            // Pip soft blurred halo
            float pipHaloR = 4.8f + pulse * 1.5f;
            g.setColour (arcCol.withAlpha (juce::jlimit (0.25f, 0.55f, 0.32f + pulse * 0.20f)));
            g.fillEllipse (pipX - pipHaloR, pipY - pipHaloR, pipHaloR * 2.0f, pipHaloR * 2.0f);
            
            // Pip core
            g.setColour (arcCol.brighter (0.25f));
            g.fillEllipse (pipX - 2.5f, pipY - 2.5f, 5.0f, 5.0f);

            // Soft White Jewel Center
            g.setColour (juce::Colour (0xffffffff).withAlpha (0.90f));
            g.fillEllipse (pipX - 1.2f, pipY - 1.2f, 2.4f, 2.4f);
        }

        // 4. Dial Knob Body (Minimalist Floating Obsidian Rotor)
        float dialRadius = radius * 0.68f;
        auto dialBounds = juce::Rectangle<float> (centre.x - dialRadius, centre.y - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f);

        // Subtle Soft Ambient Depth Shadow
        g.setColour (juce::Colour (0x99000000));
        g.fillEllipse (dialBounds.translated (0.0f, 1.6f));

        // Precision Obsidian Dial Face
        juce::ColourGradient dialGrad (juce::Colour (0xff16181f), centre.x, dialBounds.getY(),
                                       juce::Colour (0xff0e1015), centre.x, dialBounds.getBottom(), false);
        g.setGradientFill (dialGrad);
        g.fillEllipse (dialBounds);

        // Razor-thin Hairline Rim
        g.setColour (juce::Colour (0xff242732));
        g.drawEllipse (dialBounds, 1.0f);

        // Futuristic Concentric Inner Accent Ring
        float innerRingRadius = dialRadius * 0.50f;
        g.setColour (juce::Colour (0x14ffffff));
        g.drawEllipse (centre.x - innerRingRadius, centre.y - innerRingRadius,
                       innerRingRadius * 2.0f, innerRingRadius * 2.0f, 0.8f);

        // 5. Razor-Sharp Futuristic Indicator Needle (Soft pastel glow)
        float needleStart = dialRadius * 0.26f;
        float needleEnd   = dialRadius * 0.84f;

        float sinA = std::sin (toAngle);
        float cosA = std::cos (toAngle);

        float xStart = centre.x + sinA * needleStart;
        float yStart = centre.y - cosA * needleStart;
        float xEnd   = centre.x + sinA * needleEnd;
        float yEnd   = centre.y - cosA * needleEnd;

        // Needle soft blurred bloom
        g.setColour (arcCol.withAlpha (0.32f));
        g.drawLine (xStart, yStart, xEnd, yEnd, 4.0f);

        // Needle core
        g.setColour (arcCol.brighter (0.35f));
        g.drawLine (xStart, yStart, xEnd, yEnd, 1.8f);

        // Needle white jewel tip
        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (xEnd - 1.3f, yEnd - 1.3f, 2.6f, 2.6f);
    }

    // 2. Large Modern Buttons & Segmented Capsule Toggle (MID/SIDE | STEREO, BYPASS, FREEZE)
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (shouldDrawButtonAsDown);
        auto bounds = button.getLocalBounds().toFloat();
        bool isToggled = button.getToggleState();
        auto btnRect = bounds.reduced (1.0f, 1.0f);
        const float pillRadius = btnRect.getHeight() * 0.48f;

        // Special: Segmented Switch for MID / SIDE vs STEREO
        if (button.getButtonText().contains ("MID / SIDE") || button.getButtonText() == "MID / SIDE")
        {
            // Outer Capsule Well
            g.setColour (juce::Colour (0xff0a0c12));
            g.fillRoundedRectangle (btnRect, pillRadius);
            g.setColour (juce::Colour (0xff1e2330));
            g.drawRoundedRectangle (btnRect, pillRadius, 1.0f);

            float halfW = btnRect.getWidth() * 0.5f;
            auto leftSeg  = btnRect.withWidth (halfW);
            auto rightSeg = btnRect.withTrimmedLeft (halfW);

            auto activeSegRect = isToggled ? leftSeg.reduced (2.0f, 2.0f) : rightSeg.reduced (2.0f, 2.0f);
            
            // Soft active segment pill background
            juce::Colour activeTint = juce::Colour (0xff00f0ff);
            juce::ColourGradient segGrad (activeTint.withMultipliedBrightness (0.18f).overlaidWith (juce::Colour (0xff161922)), 0.0f, activeSegRect.getY(),
                                          activeTint.withMultipliedBrightness (0.08f).overlaidWith (juce::Colour (0xff0e1017)), 0.0f, activeSegRect.getBottom(), false);
            g.setGradientFill (segGrad);
            g.fillRoundedRectangle (activeSegRect, activeSegRect.getHeight() * 0.48f);

            // Soft glowing border on active segment
            g.setColour (activeTint.withAlpha (0.75f));
            g.drawRoundedRectangle (activeSegRect, activeSegRect.getHeight() * 0.48f, 1.0f);

            // Soft blurred ambient glow
            g.setColour (activeTint.withAlpha (0.12f));
            g.drawRoundedRectangle (activeSegRect.expanded (1.0f), activeSegRect.getHeight() * 0.48f, 2.5f);

            // Typography
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            
            // Left text: MID / SIDE
            g.setColour (isToggled ? juce::Colour (0xffffffff) : juce::Colour (0xff64748b));
            g.drawText ("MID / SIDE", leftSeg, juce::Justification::centred, true);

            // Right text: STEREO
            g.setColour (! isToggled ? juce::Colour (0xffffffff) : juce::Colour (0xff64748b));
            g.drawText ("STEREO", rightSeg, juce::Justification::centred, true);
            return;
        }

        // Standard Large Pill Toggle Button (BYPASS, FREEZE, etc.)
        juce::Colour activeColour = juce::Colour (0xff00f0ff);
        if (button.getButtonText() == "BYPASS")
            activeColour = juce::Colour (0xffff4455); // Soft warning crimson
        else if (button.isColourSpecified (juce::ToggleButton::tickColourId)
                 && button.findColour (juce::ToggleButton::tickColourId) != juce::Colour (0xffffffff))
            activeColour = button.findColour (juce::ToggleButton::tickColourId);

        // Sleek Capsule Background
        juce::Colour bgTop = isToggled ? activeColour.withMultipliedBrightness (0.24f).overlaidWith (juce::Colour (0xff14161f))
                                       : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff1c202c) : juce::Colour (0xff101218));
        juce::Colour bgBot = isToggled ? activeColour.withMultipliedBrightness (0.10f).overlaidWith (juce::Colour (0xff0b0d12))
                                       : juce::Colour (0xff0a0b0f);
        juce::Colour borderCol = isToggled ? activeColour.withAlpha (0.85f)
                                           : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff384257) : juce::Colour (0xff1e2330));

        juce::ColourGradient btnGrad (bgTop, 0.0f, btnRect.getY(), bgBot, 0.0f, btnRect.getBottom(), false);
        g.setGradientFill (btnGrad);
        g.fillRoundedRectangle (btnRect, pillRadius);

        // Soft ambient blurred glow underlay if active
        if (isToggled)
        {
            g.setColour (activeColour.withAlpha (0.18f));
            g.drawRoundedRectangle (btnRect.expanded (1.0f), pillRadius, 3.0f);
        }

        // Hairline border
        g.setColour (borderCol);
        g.drawRoundedRectangle (btnRect, pillRadius, 1.0f);

        // Button text (centered, bold, spacious)
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.setColour (isToggled ? juce::Colour (0xffffffff) : (shouldDrawButtonAsHighlighted ? juce::Colour (0xffe2e8f0) : juce::Colour (0xff94a3b8)));
        g.drawText (button.getButtonText(), btnRect, juce::Justification::centred, true);
    }

    // 3. Futuristic Cyber Console Fader (DRY, INPUT, WET, OUTPUT) with Soft Glow
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearVertical)
        {
            auto trackArea = juce::Rectangle<float> ((float) x + (float) width * 0.5f - 1.8f,
                                                     (float) y + 6.0f, 3.6f, (float) height - 12.0f);

            // 1. Recessed Fader Track Slot (Deep OLED Black)
            g.setColour (juce::Colour (0xff07080a));
            g.fillRoundedRectangle (trackArea, 1.8f);
            g.setColour (juce::Colour (0xff181a24));
            g.drawRoundedRectangle (trackArea, 1.8f, 1.0f);

            // 2. Minimalist Cyber Scale Ticks
            for (float ty = trackArea.getY() + 4.0f; ty <= trackArea.getBottom() - 4.0f; ty += 14.0f)
            {
                g.setColour (juce::Colour (0xff1c202a));
                g.drawHorizontalLine ((int) ty, trackArea.getX() - 5.0f, trackArea.getX() - 2.0f);
                g.drawHorizontalLine ((int) ty, trackArea.getRight() + 2.0f, trackArea.getRight() + 5.0f);
            }

            // 3. Luminous Active Beam below Fader Thumb with custom accent color
            auto activeSlot = trackArea.withTop (sliderPos);
            juce::Colour faderCol = juce::Colour (0xff00d4e8);
            if (slider.isColourSpecified (juce::Slider::thumbColourId))
            {
                auto specifiedCol = slider.findColour (juce::Slider::thumbColourId);
                if (specifiedCol != juce::Colour (0xffffffff))
                    faderCol = specifiedCol;
            }
            
            // Soft atmospheric blurred glow
            g.setColour (faderCol.withAlpha (0.16f));
            g.fillRoundedRectangle (activeSlot.expanded (3.0f, 0.0f), 3.0f);
            g.setColour (faderCol.withAlpha (0.35f));
            g.fillRoundedRectangle (activeSlot.expanded (1.2f, 0.0f), 2.0f);
            
            // Core beam
            g.setColour (faderCol);
            g.fillRoundedRectangle (activeSlot, 1.6f);

            // 4. Modern Pill Fader Cap
            float thumbW = 28.0f;
            float thumbH = 15.0f;
            auto thumbRect = juce::Rectangle<float> ((float) x + (float) width * 0.5f - thumbW * 0.5f,
                                                     sliderPos - thumbH * 0.5f, thumbW, thumbH);

            // Subtle drop shadow
            g.setColour (juce::Colour (0xaa000000));
            g.fillRoundedRectangle (thumbRect.translated (0, 1.8f), 3.0f);

            // Aerodynamic Obsidian Body
            juce::ColourGradient thumbGrad (juce::Colour (0xff252936), thumbRect.getX(), thumbRect.getY(),
                                            juce::Colour (0xff101218), thumbRect.getX(), thumbRect.getBottom(), false);
            g.setGradientFill (thumbGrad);
            g.fillRoundedRectangle (thumbRect, 3.0f);

            // Soft colored rim
            g.setColour (faderCol.withAlpha (0.65f));
            g.drawRoundedRectangle (thumbRect, 3.0f, 1.0f);

            // Center slit with soft glow
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

    // 4. Modern Preset ComboBox Dropdown Display (Capsule Shape)
    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
        const float cornerR = bounds.getHeight() * 0.48f;

        // Recessed Dark Glass Capsule Container
        juce::Colour bgTop = isButtonDown ? juce::Colour (0xff121319) : (box.isMouseOver() ? juce::Colour (0xff1b1e28) : juce::Colour (0xff11131a));
        juce::Colour bgBot = juce::Colour (0xff0a0b0f);
        juce::Colour borderCol = box.isMouseOver() ? juce::Colour (0xff00f0ff).withAlpha (0.75f) : juce::Colour (0xff222736);

        juce::ColourGradient grad (bgTop, 0.0f, bounds.getY(), bgBot, 0.0f, bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, cornerR);

        g.setColour (borderCol);
        g.drawRoundedRectangle (bounds, cornerR, 1.0f);

        // Futuristic Minimalist Chevron Arrow
        auto arrowZone = bounds.removeFromRight (24.0f);
        juce::Path arrow;
        float ax = arrowZone.getCentreX();
        float ay = arrowZone.getCentreY();
        arrow.startNewSubPath (ax - 3.5f, ay - 1.5f);
        arrow.lineTo (ax, ay + 2.0f);
        arrow.lineTo (ax + 3.5f, ay - 1.5f);

        g.setColour (box.isMouseOver() ? juce::Colour (0xffffffff) : juce::Colour (0xff94a3b8));
        g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 5. Modern Futuristic Push Buttons (A/B, Scale, Copy) - Capsule Styling
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jlimit (9.5f, 11.5f, (float) buttonHeight * 0.38f), juce::Font::bold));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const float cornerR = bounds.getHeight() * 0.48f; // Elegant Capsule Pill

        if (button.getComponentID() == "heartPresetBtn")
        {
            const bool isFav = button.getToggleState();
            juce::Colour bgTop = shouldDrawButtonAsDown ? juce::Colour (0xff14161e)
                               : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff1d202b) : juce::Colour (0xff11131a));
            juce::Colour bgBot = juce::Colour (0xff0a0b0f);
            juce::ColourGradient grad (bgTop, 0.0f, bounds.getY(), bgBot, 0.0f, bounds.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bounds, cornerR);

            juce::Colour borderCol;
            if (isFav)
                borderCol = shouldDrawButtonAsHighlighted ? juce::Colour (0xffff4d6d) : juce::Colour (0xffff3366).withAlpha (0.85f);
            else if (shouldDrawButtonAsHighlighted)
                borderCol = juce::Colour (0xffff4d6d).withAlpha (0.6f);
            else
                borderCol = juce::Colour (0xff242836);

            g.setColour (borderCol);
            g.drawRoundedRectangle (bounds, cornerR, 1.0f);
            return;
        }

        if (button.getComponentID() == "prevPresetBtn" || button.getComponentID() == "nextPresetBtn")
        {
            juce::Colour bgTop = shouldDrawButtonAsDown ? juce::Colour (0xff14161e)
                               : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff1d202b) : juce::Colour (0xff11131a));
            juce::Colour bgBot = juce::Colour (0xff0a0b0f);
            juce::ColourGradient grad (bgTop, 0.0f, bounds.getY(), bgBot, 0.0f, bounds.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bounds, cornerR);

            juce::Colour borderCol = shouldDrawButtonAsHighlighted ? juce::Colour (0xff00f0ff).withAlpha (0.75f)
                                                                    : juce::Colour (0xff242836);
            g.setColour (borderCol);
            g.drawRoundedRectangle (bounds, cornerR, 1.0f);
            return;
        }

        juce::Colour bg = backgroundColour;
        if (shouldDrawButtonAsDown)
            bg = bg.darker (0.3f);
        else if (shouldDrawButtonAsHighlighted)
            bg = bg.brighter (0.18f);

        juce::ColourGradient grad (bg.brighter (0.06f), bounds.getX(), bounds.getY(),
                                   bg.darker (0.15f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, cornerR);

        juce::Colour borderCol = shouldDrawButtonAsHighlighted ? juce::Colour (0xff00f0ff).withAlpha (0.75f)
                                                                : juce::Colour (0xff242836);
        g.setColour (borderCol);
        g.drawRoundedRectangle (bounds, cornerR, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
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
                g.setColour (juce::Colour (0x33ff3366));
                g.strokePath (heart, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (shouldDrawButtonAsHighlighted ? juce::Colour (0xffff6688) : juce::Colour (0xffff3366));
                g.fillPath (heart);
            }
            else
            {
                juce::Colour strokeCol = shouldDrawButtonAsHighlighted ? juce::Colour (0xffff6688) : juce::Colour (0xff94a3b8);
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
            g.setColour (shouldDrawButtonAsHighlighted ? juce::Colour (0xffffffff) : juce::Colour (0xff94a3b8));
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
            g.setColour (shouldDrawButtonAsHighlighted ? juce::Colour (0xffffffff) : juce::Colour (0xff94a3b8));
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
