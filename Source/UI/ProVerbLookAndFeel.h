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

    // 1. Hardware 3D Rotary Knobs (FL Studio-Inspired Tactile Dials with Glowing Cyan Arc)
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& /*slider*/) override
    {
        auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
        auto centre = area.getCentre();
        
        float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.44f;
        if (radius < 10.0f)
            return;

        auto squareBounds = juce::Rectangle<float> (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // 1. Recessed Socket Well (Dark Carved Socket with Top-Left Bevel Highlight)
        g.setColour (juce::Colour (0xff0a0b0e));
        g.fillEllipse (squareBounds);

        // Outer socket bevel (light catch on top, shadow on bottom)
        juce::ColourGradient socketGrad (juce::Colour (0xff2b2e38), centre.x, squareBounds.getY(),
                                         juce::Colour (0xff0d0e12), centre.x, squareBounds.getBottom(), false);
        g.setGradientFill (socketGrad);
        g.drawEllipse (squareBounds, 1.2f);

        // 2. Track Background Arc (Recessed Dark Channel)
        float arcRadius = radius - 4.5f;
        juce::Path bgArc;
        bgArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xff16171d));
        g.strokePath (bgArc, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Active Glowing Value Arc (Signature Hardware Cyan Neon Glow)
        if (sliderPosProportional > 0.001f)
        {
            juce::Path activeArc;
            activeArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                     rotaryStartAngle, toAngle, true);
            
            // Soft Ambient Cyan Bloom
            g.setColour (juce::Colour (0x3300f0ff));
            g.strokePath (activeArc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Vivid Core Arc (Ice Cyan #00e5ff to #38bdf8 gradient)
            juce::ColourGradient arcGrad (juce::Colour (0xff00e5ff), squareBounds.getX(), squareBounds.getY(),
                                           juce::Colour (0xff38bdf8), squareBounds.getRight(), squareBounds.getBottom(), false);
            g.setGradientFill (arcGrad);
            g.strokePath (activeArc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 4. Dial Knob Body (Raised 3D Hardware Finish)
        float dialRadius = radius * 0.72f;
        auto dialBounds = juce::Rectangle<float> (centre.x - dialRadius, centre.y - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f);

        // Tactile Drop Shadow Underneath Dial
        g.setColour (juce::Colour (0x88000000));
        g.fillEllipse (dialBounds.translated (0.0f, 2.5f));

        // Outer Precision Bevel (Light Source from Top-Left: #565a6b down to #191a20)
        juce::ColourGradient bevelGrad (juce::Colour (0xff545868), centre.x - dialRadius * 0.7f, centre.y - dialRadius * 0.7f,
                                        juce::Colour (0xff18191f), centre.x + dialRadius * 0.7f, centre.y + dialRadius * 0.7f, false);
        g.setGradientFill (bevelGrad);
        g.fillEllipse (dialBounds);

        // Dial Face (ProducerToy Signature Slate #34363d Hardware Finish)
        float faceRadius = dialRadius - 1.8f;
        auto faceBounds = juce::Rectangle<float> (centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);
        juce::ColourGradient faceGrad (juce::Colour (0xff424652), centre.x, faceBounds.getY(),
                                       juce::Colour (0xff22232a), centre.x, faceBounds.getBottom(), false);
        g.setGradientFill (faceGrad);
        g.fillEllipse (faceBounds);

        // Fine Concentric Milled Groove Ring
        g.setColour (juce::Colour (0x33000000));
        g.drawEllipse (faceBounds.reduced (2.5f), 1.0f);

        // Inner Brushed Cap (Deep Graphite Inset Core)
        float capRadius = dialRadius * 0.42f;
        auto capBounds = juce::Rectangle<float> (centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);
        juce::ColourGradient capGrad (juce::Colour (0xff2c2e36), centre.x, capBounds.getY(),
                                      juce::Colour (0xff141519), centre.x, capBounds.getBottom(), false);
        g.setGradientFill (capGrad);
        g.fillEllipse (capBounds);

        g.setColour (juce::Colour (0xff383b46));
        g.drawEllipse (capBounds, 1.0f);

        // 5. Crisp Hardware Pointer Indicator with Illuminated Tip
        juce::Path pointer;
        float ptrLen = dialRadius * 0.75f;
        pointer.addRoundedRectangle (-1.5f, -dialRadius + 2.0f, 3.0f, ptrLen * 0.55f, 1.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (toAngle).translated (centre.x, centre.y));

        g.setColour (juce::Colour (0xffffffff));
        g.fillPath (pointer);

        // Cyan Glowing Tip Dot at Dial Outer Edge
        float tipX = centre.x + std::sin (toAngle) * (dialRadius - 2.5f);
        float tipY = centre.y - std::cos (toAngle) * (dialRadius - 2.5f);
        g.setColour (juce::Colour (0xff38bdf8));
        g.fillEllipse (tipX - 1.5f, tipY - 1.5f, 3.0f, 3.0f);
    }

    // 2. Hardware Tactile Push-Buttons with Illuminated LEDs (MID/SIDE, FREEZE, BYPASS)
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (shouldDrawButtonAsDown);
        auto bounds = button.getLocalBounds().toFloat();
        bool isToggled = button.getToggleState();

        auto btnRect = bounds.reduced (2.0f, 2.0f);
        const float cornerR = 4.0f;

        // Custom accent color: Studio warning Amber-Red for BYPASS, Ice-Cyan for FREEZE & MID/SIDE
        juce::Colour activeColour = juce::Colour (0xff00f0ff);
        if (button.getButtonText() == "BYPASS")
            activeColour = juce::Colour (0xffff4444);
        else if (button.isColourSpecified (juce::ToggleButton::tickColourId)
                 && button.findColour (juce::ToggleButton::tickColourId) != juce::Colour (0xffffffff))
            activeColour = button.findColour (juce::ToggleButton::tickColourId);

        // Button chassis background
        juce::Colour bgTop = isToggled ? activeColour.withMultipliedBrightness (0.24f).overlaidWith (juce::Colour (0xff16171d))
                                       : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff24262f) : juce::Colour (0xff191a21));
        juce::Colour bgBot = isToggled ? activeColour.withMultipliedBrightness (0.12f).overlaidWith (juce::Colour (0xff0e0f13))
                                       : juce::Colour (0xff121317);
        juce::Colour borderCol = isToggled ? activeColour.withAlpha (0.85f)
                                           : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff454a57) : juce::Colour (0xff2b2d36));

        juce::ColourGradient btnGrad (bgTop, 0.0f, btnRect.getY(), bgBot, 0.0f, btnRect.getBottom(), false);
        g.setGradientFill (btnGrad);
        g.fillRoundedRectangle (btnRect, cornerR);

        // Top bevel highlight
        g.setColour (isToggled ? activeColour.withAlpha (0.4f) : juce::Colour (0x1affffff));
        g.drawHorizontalLine ((int) btnRect.getY() + 1, btnRect.getX() + 3.0f, btnRect.getRight() - 3.0f);

        // Outer border
        g.setColour (borderCol);
        g.drawRoundedRectangle (btnRect, cornerR, 1.0f);

        // Hardware LED Indicator Dot (Left side)
        float ledSize = 6.0f;
        float ledX = btnRect.getX() + 8.0f;
        float ledY = btnRect.getCentreY() - ledSize * 0.5f;

        if (isToggled)
        {
            // Glowing LED with bloom
            g.setColour (activeColour.withAlpha (0.35f));
            g.fillEllipse (ledX - 2.5f, ledY - 2.5f, ledSize + 5.0f, ledSize + 5.0f);

            g.setColour (activeColour);
            g.fillEllipse (ledX, ledY, ledSize, ledSize);

            g.setColour (juce::Colour (0xffffffff));
            g.fillEllipse (ledX + 1.5f, ledY + 1.0f, 2.2f, 2.2f);
        }
        else
        {
            // Dark recessed unlit LED socket
            g.setColour (juce::Colour (0xff0c0d10));
            g.fillEllipse (ledX, ledY, ledSize, ledSize);

            // Subtle dormant tint if BYPASS lamp
            if (button.getButtonText() == "BYPASS")
            {
                g.setColour (juce::Colour (0x33441515));
                g.fillEllipse (ledX + 1.0f, ledY + 1.0f, ledSize - 2.0f, ledSize - 2.0f);
            }

            g.setColour (juce::Colour (0xff282b34));
            g.drawEllipse (ledX, ledY, ledSize, ledSize, 1.0f);
        }

        // Button Text — Centered in the available text bay with full width clearance
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.setColour (isToggled ? juce::Colour (0xffffffff) : (shouldDrawButtonAsHighlighted ? juce::Colour (0xffcbd5e1) : juce::Colour (0xff8f929e)));
        float ledRight = ledX + ledSize;
        auto textRect = btnRect.withTrimmedLeft (ledRight + 4.0f - btnRect.getX()).withTrimmedRight (4.0f);
        g.drawText (button.getButtonText(), textRect, juce::Justification::centred, true);
    }

    // 3. Hardware Studio Vertical Console Fader (DRY, INPUT, WET)
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearVertical)
        {
            auto trackArea = juce::Rectangle<float> ((float) x + (float) width * 0.5f - 2.5f,
                                                     (float) y + 6.0f, 5.0f, (float) height - 12.0f);

            // 1. Recessed Fader Milled Slot
            g.setColour (juce::Colour (0xff090a0d));
            g.fillRoundedRectangle (trackArea, 2.5f);
            g.setColour (juce::Colour (0xff1c1e24));
            g.drawRoundedRectangle (trackArea, 2.5f, 1.0f);

            // 2. Hardware Precision Scale Ticks on Left & Right
            for (float ty = trackArea.getY() + 4.0f; ty <= trackArea.getBottom() - 4.0f; ty += 12.0f)
            {
                g.setColour (juce::Colour (0xff262831));
                g.drawHorizontalLine ((int) ty, trackArea.getX() - 6.0f, trackArea.getX() - 2.0f);
                g.drawHorizontalLine ((int) ty, trackArea.getRight() + 2.0f, trackArea.getRight() + 6.0f);
            }

            // 3. Illuminated Active Slot below Fader Thumb
            auto activeSlot = trackArea.withTop (sliderPos);
            g.setColour (juce::Colour (0x5538bdf8));
            g.fillRoundedRectangle (activeSlot, 2.0f);
            g.setColour (juce::Colour (0xff00d4e8));
            g.drawHorizontalLine ((int) sliderPos, trackArea.getX(), trackArea.getRight());

            // 4. Hardware Studio Console Fader Cap
            float thumbW = 28.0f;
            float thumbH = 18.0f;
            auto thumbRect = juce::Rectangle<float> ((float) x + (float) width * 0.5f - thumbW * 0.5f,
                                                     sliderPos - thumbH * 0.5f, thumbW, thumbH);

            // Drop shadow
            g.setColour (juce::Colour (0x88000000));
            g.fillRoundedRectangle (thumbRect.translated (0, 2.5f), 3.0f);

            // Tactile Fader Cap Gradient (#34363d Hardware Theme)
            juce::ColourGradient thumbGrad (juce::Colour (0xff4b4e5b), thumbRect.getX(), thumbRect.getY(),
                                            juce::Colour (0xff23252c), thumbRect.getX(), thumbRect.getBottom(), false);
            g.setGradientFill (thumbGrad);
            g.fillRoundedRectangle (thumbRect, 3.0f);

            // Bevel rim
            g.setColour (juce::Colour (0xff595d6c));
            g.drawRoundedRectangle (thumbRect, 3.0f, 1.0f);

            // Horizontal tactile grip grooves on cap
            g.setColour (juce::Colour (0xff1a1b21));
            g.drawHorizontalLine ((int) thumbRect.getCentreY() - 4, thumbRect.getX() + 4.0f, thumbRect.getRight() - 4.0f);
            g.drawHorizontalLine ((int) thumbRect.getCentreY() + 4, thumbRect.getX() + 4.0f, thumbRect.getRight() - 4.0f);

            // Center Clean White Indicator Line
            g.setColour (juce::Colour (0xffffffff));
            g.fillRect (thumbRect.getX() + 3.0f, thumbRect.getCentreY() - 1.0f, thumbW - 6.0f, 2.0f);
        }
        else
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }

    // 4. Hardware Preset ComboBox Dropdown Display
    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
        const float cornerR = 5.0f;

        // Recessed Hardware Display Box
        juce::Colour bgTop = isButtonDown ? juce::Colour (0xff16171d) : (box.isMouseOver() ? juce::Colour (0xff21232c) : juce::Colour (0xff181920));
        juce::Colour bgBot = juce::Colour (0xff111216);
        juce::Colour borderCol = box.isMouseOver() ? juce::Colour (0xff38bdf8).withAlpha (0.7f) : juce::Colour (0xff2d303a);

        juce::ColourGradient grad (bgTop, 0.0f, bounds.getY(), bgBot, 0.0f, bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, cornerR);

        g.setColour (borderCol);
        g.drawRoundedRectangle (bounds, cornerR, 1.0f);

        // Hardware Dropdown Arrow (Crisp Silver Glyph)
        auto arrowZone = bounds.removeFromRight (22.0f);
        juce::Path arrow;
        float ax = arrowZone.getCentreX();
        float ay = arrowZone.getCentreY();
        arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
        arrow.lineTo (ax + 4.0f, ay - 2.0f);
        arrow.lineTo (ax, ay + 3.0f);
        arrow.closeSubPath();

        g.setColour (box.isMouseOver() ? juce::Colour (0xffffffff) : juce::Colour (0xff94a3b8));
        g.fillPath (arrow);
    }

    // 5. Hardware Push Buttons (A/B, Scale, Copy)
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
            bg = bg.darker (0.25f);
        else if (shouldDrawButtonAsHighlighted)
            bg = bg.brighter (0.15f);

        juce::ColourGradient grad (bg.brighter (0.08f), bounds.getX(), bounds.getY(),
                                   bg.darker (0.12f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, cornerR);

        juce::Colour borderCol = shouldDrawButtonAsHighlighted ? juce::Colour (0xff38bdf8).withAlpha (0.8f)
                                                                : juce::Colour (0xff2d303a);
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
