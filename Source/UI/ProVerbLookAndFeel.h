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
        juce::ColourGradient socketGrad (juce::Colour (0xff1f232f), centre.x, squareBounds.getY(),
                                         juce::Colour (0xff0c0d12), centre.x, squareBounds.getBottom(), false);
        g.setGradientFill (socketGrad);
        g.drawEllipse (squareBounds, 1.0f);

        // 2. Track Background Arc (Minimalist Dark Hairline Slot)
        float arcRadius = radius - 4.2f;
        juce::Path bgArc;
        bgArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xff151722));
        g.strokePath (bgArc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Active Luminous Value Arc (Futuristic Glowing Neon with custom accent colour)
        juce::Colour arcCol = juce::Colour (0xff00f0ff);
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
            
            // Soft Ambient Neon Bloom
            g.setColour (arcCol.withAlpha (0.28f));
            g.strokePath (activeArc, juce::PathStrokeType (6.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Sharp Luminous Laser Core
            juce::ColourGradient arcGrad (arcCol, squareBounds.getX(), squareBounds.getY(),
                                          arcCol.brighter (0.25f), squareBounds.getRight(), squareBounds.getBottom(), false);
            g.setGradientFill (arcGrad);
            g.strokePath (activeArc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Radiant Active Beacon Indicator Pip at current value
            float pipX = centre.x + std::sin (toAngle) * arcRadius;
            float pipY = centre.y - std::cos (toAngle) * arcRadius;
            
            // Pip Neon Halo
            g.setColour (arcCol.withAlpha (0.55f));
            g.fillEllipse (pipX - 3.5f, pipY - 3.5f, 7.0f, 7.0f);
            
            // Pure White Laser Core Dot
            g.setColour (juce::Colour (0xffffffff));
            g.fillEllipse (pipX - 1.5f, pipY - 1.5f, 3.0f, 3.0f);
        }

        // 4. Dial Knob Body (Minimalist Floating Obsidian Rotor)
        float dialRadius = radius * 0.68f;
        auto dialBounds = juce::Rectangle<float> (centre.x - dialRadius, centre.y - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f);

        // Subtle Soft Ambient Depth Shadow
        g.setColour (juce::Colour (0xaa000000));
        g.fillEllipse (dialBounds.translated (0.0f, 1.8f));

        // Precision Obsidian Dial Face
        juce::ColourGradient dialGrad (juce::Colour (0xff1a1c25), centre.x, dialBounds.getY(),
                                       juce::Colour (0xff0e0f15), centre.x, dialBounds.getBottom(), false);
        g.setGradientFill (dialGrad);
        g.fillEllipse (dialBounds);

        // Razor-sharp Hairline Rim
        g.setColour (juce::Colour (0xff2b3040));
        g.drawEllipse (dialBounds, 1.0f);

        // Futuristic Concentric Inner Accent Ring
        float innerRingRadius = dialRadius * 0.48f;
        g.setColour (juce::Colour (0x18ffffff));
        g.drawEllipse (centre.x - innerRingRadius, centre.y - innerRingRadius,
                       innerRingRadius * 2.0f, innerRingRadius * 2.0f, 0.8f);

        // 5. Razor-Sharp Futuristic Indicator Needle
        float needleStart = dialRadius * 0.28f;
        float needleEnd   = dialRadius * 0.84f;

        float sinA = std::sin (toAngle);
        float cosA = std::cos (toAngle);

        float xStart = centre.x + sinA * needleStart;
        float yStart = centre.y - cosA * needleStart;
        float xEnd   = centre.x + sinA * needleEnd;
        float yEnd   = centre.y - cosA * needleEnd;

        // Needle subtle bloom with custom accent
        g.setColour (arcCol.withAlpha (0.44f));
        g.drawLine (xStart, yStart, xEnd, yEnd, 3.2f);

        // Sharp needle core
        g.setColour (juce::Colour (0xffffffff));
        g.drawLine (xStart, yStart, xEnd, yEnd, 1.5f);
    }

    // 2. Futuristic Tactile Push-Buttons with High-Tech Micro-LED (MID/SIDE, FREEZE, BYPASS)
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (shouldDrawButtonAsDown);
        auto bounds = button.getLocalBounds().toFloat();
        bool isToggled = button.getToggleState();

        auto btnRect = bounds.reduced (1.5f, 1.5f);
        const float cornerR = 4.0f;

        // Custom accent color: Studio warning Crimson for BYPASS, Electric Cyan for FREEZE & MID/SIDE
        juce::Colour activeColour = juce::Colour (0xff00f0ff);
        if (button.getButtonText() == "BYPASS")
            activeColour = juce::Colour (0xffff3855);
        else if (button.isColourSpecified (juce::ToggleButton::tickColourId)
                 && button.findColour (juce::ToggleButton::tickColourId) != juce::Colour (0xffffffff))
            activeColour = button.findColour (juce::ToggleButton::tickColourId);

        // Futuristic chassis background
        juce::Colour bgTop = isToggled ? activeColour.withMultipliedBrightness (0.22f).overlaidWith (juce::Colour (0xff12141c))
                                       : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff1e212b) : juce::Colour (0xff121319));
        juce::Colour bgBot = isToggled ? activeColour.withMultipliedBrightness (0.10f).overlaidWith (juce::Colour (0xff0b0c10))
                                       : juce::Colour (0xff0b0c10);
        juce::Colour borderCol = isToggled ? activeColour.withAlpha (0.90f)
                                           : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff3d4455) : juce::Colour (0xff222530));

        juce::ColourGradient btnGrad (bgTop, 0.0f, btnRect.getY(), bgBot, 0.0f, btnRect.getBottom(), false);
        g.setGradientFill (btnGrad);
        g.fillRoundedRectangle (btnRect, cornerR);

        // Subtle cyber glow underlay if active
        if (isToggled)
        {
            g.setColour (activeColour.withAlpha (0.15f));
            g.fillRoundedRectangle (btnRect, cornerR);
        }

        // Razor-thin border
        g.setColour (borderCol);
        g.drawRoundedRectangle (btnRect, cornerR, 1.0f);

        // High-Tech Micro-LED Indicator (Left side)
        float ledSize = 5.0f;
        float ledX = btnRect.getX() + 8.0f;
        float ledY = btnRect.getCentreY() - ledSize * 0.5f;

        if (isToggled)
        {
            // Radiant Micro-LED Bloom
            g.setColour (activeColour.withAlpha (0.45f));
            g.fillEllipse (ledX - 3.0f, ledY - 3.0f, ledSize + 6.0f, ledSize + 6.0f);

            g.setColour (activeColour);
            g.fillEllipse (ledX, ledY, ledSize, ledSize);

            // Laser White Core
            g.setColour (juce::Colour (0xffffffff));
            g.fillEllipse (ledX + 1.2f, ledY + 1.0f, 2.0f, 2.0f);
        }
        else
        {
            // Dark recessed unlit socket
            g.setColour (juce::Colour (0xff08090c));
            g.fillEllipse (ledX, ledY, ledSize, ledSize);

            if (button.getButtonText() == "BYPASS")
            {
                g.setColour (juce::Colour (0x33441515));
                g.fillEllipse (ledX + 1.0f, ledY + 1.0f, ledSize - 2.0f, ledSize - 2.0f);
            }

            g.setColour (juce::Colour (0xff252834));
            g.drawEllipse (ledX, ledY, ledSize, ledSize, 0.8f);
        }

        // Clean Modern Button Typography
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.setColour (isToggled ? juce::Colour (0xffffffff) : (shouldDrawButtonAsHighlighted ? juce::Colour (0xffcbd5e1) : juce::Colour (0xff8f94a6)));
        float ledRight = ledX + ledSize;
        auto textRect = btnRect.withTrimmedLeft (ledRight + 4.0f - btnRect.getX()).withTrimmedRight (4.0f);
        g.drawText (button.getButtonText(), textRect, juce::Justification::centred, true);
    }

    // 3. Futuristic Cyber Console Fader (DRY, INPUT, WET, OUTPUT)
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
                g.setColour (juce::Colour (0xff20232d));
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
            
            // Soft ambient glow
            g.setColour (faderCol.withAlpha (0.35f));
            g.fillRoundedRectangle (activeSlot.expanded (1.0f, 0.0f), 2.0f);
            
            // Laser beam core
            g.setColour (faderCol);
            g.fillRoundedRectangle (activeSlot, 1.5f);

            // 4. Futuristic Minimalist Cyber Fader Cap
            float thumbW = 26.0f;
            float thumbH = 15.0f;
            auto thumbRect = juce::Rectangle<float> ((float) x + (float) width * 0.5f - thumbW * 0.5f,
                                                     sliderPos - thumbH * 0.5f, thumbW, thumbH);

            // Subtle drop shadow
            g.setColour (juce::Colour (0xaa000000));
            g.fillRoundedRectangle (thumbRect.translated (0, 2.0f), 2.5f);

            // Aerodynamic Obsidian Body
            juce::ColourGradient thumbGrad (juce::Colour (0xff252834), thumbRect.getX(), thumbRect.getY(),
                                            juce::Colour (0xff101117), thumbRect.getX(), thumbRect.getBottom(), false);
            g.setGradientFill (thumbGrad);
            g.fillRoundedRectangle (thumbRect, 2.5f);

            // Hairline Cyber Rim
            g.setColour (faderCol.withAlpha (0.6f));
            g.drawRoundedRectangle (thumbRect, 2.5f, 1.0f);

            // Glowing Center Neon Laser Slit
            g.setColour (faderCol.withAlpha (0.75f));
            g.drawHorizontalLine ((int) thumbRect.getCentreY(), thumbRect.getX() + 3.0f, thumbRect.getRight() - 3.0f);
            
            g.setColour (juce::Colour (0xffffffff));
            g.fillRect (thumbRect.getX() + 5.0f, thumbRect.getCentreY() - 0.75f, thumbW - 10.0f, 1.5f);
        }
        else
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }

    // 4. Modern Preset ComboBox Dropdown Display
    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
        const float cornerR = 5.0f;

        // Recessed Dark Glass Container
        juce::Colour bgTop = isButtonDown ? juce::Colour (0xff121319) : (box.isMouseOver() ? juce::Colour (0xff1c1e27) : juce::Colour (0xff13141b));
        juce::Colour bgBot = juce::Colour (0xff0a0b0f);
        juce::Colour borderCol = box.isMouseOver() ? juce::Colour (0xff00f0ff).withAlpha (0.7f) : juce::Colour (0xff252936);

        juce::ColourGradient grad (bgTop, 0.0f, bounds.getY(), bgBot, 0.0f, bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, cornerR);

        g.setColour (borderCol);
        g.drawRoundedRectangle (bounds, cornerR, 1.0f);

        // Futuristic Minimalist Chevron Arrow
        auto arrowZone = bounds.removeFromRight (22.0f);
        juce::Path arrow;
        float ax = arrowZone.getCentreX();
        float ay = arrowZone.getCentreY();
        arrow.startNewSubPath (ax - 3.5f, ay - 1.5f);
        arrow.lineTo (ax, ay + 2.0f);
        arrow.lineTo (ax + 3.5f, ay - 1.5f);

        g.setColour (box.isMouseOver() ? juce::Colour (0xffffffff) : juce::Colour (0xff94a3b8));
        g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 5. Modern Futuristic Push Buttons (A/B, Scale, Copy)
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jlimit (9.5f, 11.5f, (float) buttonHeight * 0.38f), juce::Font::bold));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const float cornerR = 4.0f;

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
                                                                : juce::Colour (0xff272b38);
        g.setColour (borderCol);
        g.drawRoundedRectangle (bounds, cornerR, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) override
    {
        auto font = getTextButtonFont (button, button.getHeight());
        g.setFont (font);
        g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                               : juce::TextButton::textColourOffId)
                            .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));

        g.drawText (button.getButtonText(), button.getLocalBounds(),
                    juce::Justification::centred, false);
    }
};
