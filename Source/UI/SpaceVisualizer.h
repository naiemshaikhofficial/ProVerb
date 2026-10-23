#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>

/**
 * SpaceVisualizer — Interactive 3D Acoustic Space Visualizer
 * ProVerb by Producer Toy
 *
 * Implements the centerpiece hero acoustic panel inspired by reference image:
 * - 3D / 2D View Mode Perspective Toggle
 * - Luminous multi-gradient rounded-triangular acoustic field (Cyan, Magenta, Mint, Amber)
 * - Concentric acoustic resonance wavefront contours & layered ghost hulls
 * - Satellite orbital rings
 * - Interactive BRIGHT / WARM / SPACIOUS barycentric control puck
 * - Bottom rainbow Space slider and real-time DSP stereo phase correlation indicator
 */
class SpaceVisualizer : public juce::Component
{
public:
    SpaceVisualizer()
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    std::function<void(float bright, float warm, float spacious)> onAcousticBalanceChanged;
    std::function<void(float newSizePercent)> onSpaceSizeChanged;

    void updateAllParameters (float newRmsIn, float newRmsOut,
                              float sizePercent, float decaySec,
                              float diffusionPercent, float dampingPercent,
                              float predelayMs, float shimmerPercent,
                              float newPhaseCorr = 1.0f,
                              float lowCutHz = 120.0f, float highCutHz = 12000.0f,
                              float duckPercent = 0.0f)
    {
        rmsIn = newRmsIn;
        rmsOut = newRmsOut;
        spaceSize = juce::jlimit (10.0f, 100.0f, sizePercent);
        decayTime = decaySec;
        diffusion = diffusionPercent;
        damping = dampingPercent;
        predelay = predelayMs;
        shimmer = shimmerPercent;
        phaseCorr = newPhaseCorr;
        lowCut = lowCutHz;
        highCut = highCutHz;
        ducking = duckPercent;

        if (! isDraggingPuck)
        {
            float targetBright = juce::jlimit (0.05f, 0.90f, (100.0f - damping) / 100.0f * 0.6f + (spaceSize / 100.0f) * 0.4f);
            float targetWarm   = juce::jlimit (0.05f, 0.90f, (damping / 100.0f) * 0.7f + (1.0f - diffusion / 100.0f) * 0.3f);
            float targetSpace  = juce::jlimit (0.05f, 0.90f, (spaceSize / 100.0f) * 0.6f + (diffusion / 100.0f) * 0.4f);

            float totalW = targetBright + targetWarm + targetSpace;
            if (totalW > 0.001f)
            {
                targetBright /= totalW;
                targetWarm   /= totalW;
                targetSpace  /= totalW;
            }

            weightBright += (targetBright - weightBright) * 0.15f;
            weightWarm   += (targetWarm   - weightWarm)   * 0.15f;
            weightSpace  += (targetSpace  - weightSpace)  * 0.15f;
        }

        repaint();
    }

    void setAnalyzing (bool analyzing, float progress = 0.0f)
    {
        isAnalyzing = analyzing;
        analyzeProgress = progress;
        repaint();
    }

    void advanceAnimation (float deltaPhase)
    {
        rotationAngle += deltaPhase * 0.35f;
        if (rotationAngle > juce::MathConstants<float>::twoPi)
            rotationAngle -= juce::MathConstants<float>::twoPi;

        shimmerPhase += deltaPhase * 1.5f;
        if (shimmerPhase > juce::MathConstants<float>::twoPi)
            shimmerPhase -= juce::MathConstants<float>::twoPi;

        contourPulsePhase += deltaPhase * 1.6f;
        if (contourPulsePhase > juce::MathConstants<float>::twoPi)
            contourPulsePhase -= juce::MathConstants<float>::twoPi;

        colorMorphPhase += deltaPhase * 0.5f;
        if (colorMorphPhase > juce::MathConstants<float>::twoPi * 10.0f)
            colorMorphPhase -= juce::MathConstants<float>::twoPi * 10.0f;

        repaint();
    }

    struct Geometry
    {
        juce::Point<float> centre;
        float radius { 100.0f };
        float scaleY { 1.0f };
        juce::Point<float> vBright;
        juce::Point<float> vWarm;
        juce::Point<float> vSpacious;
        juce::Point<float> puckPos;
    };

    Geometry getGeometry (juce::Rectangle<float> bounds) const
    {
        Geometry geom;
        geom.scaleY = is3DMode ? 0.88f : 1.0f;

        geom.centre = juce::Point<float> (bounds.getCentreX(), bounds.getY() + bounds.getHeight() * 0.46f);

        float minDim = juce::jmin (bounds.getWidth(), bounds.getHeight());
        geom.radius = minDim * 0.38f;

        geom.vBright   = getVertexPosition (geom.centre, geom.radius, geom.scaleY, 0);
        geom.vWarm     = getVertexPosition (geom.centre, geom.radius, geom.scaleY, 1);
        geom.vSpacious = getVertexPosition (geom.centre, geom.radius, geom.scaleY, 2);

        geom.puckPos = juce::Point<float> (
            weightBright * geom.vBright.x + weightWarm * geom.vWarm.x + weightSpace * geom.vSpacious.x,
            weightBright * geom.vBright.y + weightWarm * geom.vWarm.y + weightSpace * geom.vSpacious.y
        );

        return geom;
    }

    juce::Colour getLiveColorBright() const
    {
        float morph = (std::sin (colorMorphPhase * 0.7f) + 1.0f) * 0.5f;
        auto c1 = juce::Colour (0xff24d7ff); // Cyan
        auto c2 = juce::Colour (0xff357dff); // Blue
        return c1.interpolatedWith (c2, morph * 0.35f);
    }

    juce::Colour getLiveColorWarm() const
    {
        float morph = (std::cos (colorMorphPhase * 0.6f + 1.2f) + 1.0f) * 0.5f;
        auto c1 = juce::Colour (0xfff34db7); // Magenta / Pink
        auto c2 = juce::Colour (0xffff5a5f); // Coral
        return c1.interpolatedWith (c2, morph * 0.30f);
    }

    juce::Colour getLiveColorSpacious() const
    {
        float morph = (std::sin (colorMorphPhase * 0.65f + 2.4f) + 1.0f) * 0.5f;
        auto c1 = juce::Colour (0xff35e6b5); // Mint Green
        auto c2 = juce::Colour (0xff24d7ff); // Cyan
        return c1.interpolatedWith (c2, morph * 0.30f);
    }

    juce::Colour getLiveColorAmber() const
    {
        float morph = (std::cos (colorMorphPhase * 0.5f + 3.1f) + 1.0f) * 0.5f;
        auto c1 = juce::Colour (0xffffb451); // Amber
        auto c2 = juce::Colour (0xffff8b3d); // Peach
        return c1.interpolatedWith (c2, morph * 0.28f);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Panel Background
        juce::ColourGradient bgGrad (juce::Colour (0xff0d1521), bounds.getX(), bounds.getY(),
                                     juce::Colour (0xff0a111b), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (bgGrad);
        g.fillRoundedRectangle (bounds, 8.0f);

        // Fine cool-gray border
        g.setColour (juce::Colour (0xff243246));
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

        // 2. Panel Header: "| ACOUSTIC SPACE" on left, [ 3D VIEW | 2D VIEW ] on right
        drawHeader (g, bounds);

        auto geom = getGeometry (bounds);

        auto colBright   = getLiveColorBright();
        auto colWarm     = getLiveColorWarm();
        auto colSpacious = getLiveColorSpacious();
        auto colAmber    = getLiveColorAmber();

        // 3. Ambient Colored Atmospheric Glows
        drawAtmosphericNebula (g, geom.centre, geom.radius, geom.vBright, geom.vWarm, geom.vSpacious, geom.puckPos,
                               colBright, colWarm, colSpacious, colAmber);

        // 4. Concentric Layered Ghost Echo Hulls
        drawLayeredGhostHulls (g, geom.centre, geom.vBright, geom.vWarm, geom.vSpacious,
                               colBright, colWarm, colSpacious);

        // 5. Radar Orbital Rings & Satellites
        drawOrbitalRings (g, geom.centre, geom.radius, geom.scaleY, colBright, colWarm, colSpacious, colAmber);

        // 6. Resonance Wavefront Contours
        drawResonanceContours (g, geom.vBright, geom.vWarm, geom.vSpacious, geom.puckPos, geom.radius);

        // 7. Luminous Outer Shell Rim
        drawAcousticShell (g, geom.vBright, geom.vWarm, geom.vSpacious);

        // 8. Shimmer Floating Particles
        if (shimmer > 1.0f)
        {
            drawShimmerParticles (g, geom.centre, geom.radius, geom.scaleY);
        }

        // 9. Vertex Anchor Badges (BRIGHT, WARM, SPACIOUS)
        drawVertexNode (g, geom.vBright,   "BRIGHT",   colBright,   weightBright);
        drawVertexNode (g, geom.vWarm,     "WARM",     colWarm,     weightWarm);
        drawVertexNode (g, geom.vSpacious, "SPACIOUS", colSpacious, weightSpace);

        // 10. Central Control Puck
        drawControlPuck (g, geom.puckPos);

        // 11. Bottom Bar: SPACE Slider & Real-Time Phase Correlation Meter
        drawBottomBar (g, bounds);

        // 12. AI Acoustic Scan Overlay (if analyzing)
        if (isAnalyzing)
        {
            drawAnalysisOverlay (g, geom.centre, geom.radius);
        }
    }

    void drawHeader (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        float headY = bounds.getY() + 10.0f;
        float headX = bounds.getX() + 14.0f;

        // Vertical cyan accent bar
        g.setColour (juce::Colour (0xff24d7ff));
        g.fillRoundedRectangle (headX, headY + 2.0f, 2.5f, 11.0f, 1.0f);

        // Section Title
        g.setFont (juce::FontOptions ("Segoe UI", 10.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xfff2f6fc));
        g.drawText ("ACOUSTIC SPACE", (int) (headX + 8.0f), (int) headY, 150, 15, juce::Justification::centredLeft);

        // Right side: 3D VIEW / 2D VIEW mode switch buttons
        auto rect3D = get3DViewButtonBounds (bounds);
        auto rect2D = get2DViewButtonBounds (bounds);

        // 3D VIEW Button
        if (is3DMode)
        {
            g.setColour (juce::Colour (0xff122336));
            g.fillRoundedRectangle (rect3D, rect3D.getHeight() * 0.5f);
            g.setColour (juce::Colour (0xff24d7ff));
            g.drawRoundedRectangle (rect3D, rect3D.getHeight() * 0.5f, 1.0f);
            g.setColour (juce::Colour (0xff24d7ff).withAlpha (0.25f));
            g.drawRoundedRectangle (rect3D.expanded (1.0f), rect3D.getHeight() * 0.5f, 2.0f);
            g.setFont (juce::FontOptions ("Segoe UI", 8.5f, juce::Font::bold));
            g.setColour (juce::Colour (0xfff2f6fc));
            g.drawText ("3D VIEW", rect3D, juce::Justification::centred);
        }
        else
        {
            g.setColour (juce::Colour (0xff0a111b));
            g.fillRoundedRectangle (rect3D, rect3D.getHeight() * 0.5f);
            g.setColour (juce::Colour (0xff243246));
            g.drawRoundedRectangle (rect3D, rect3D.getHeight() * 0.5f, 1.0f);
            g.setFont (juce::FontOptions ("Segoe UI", 8.5f, juce::Font::bold));
            g.setColour (juce::Colour (0xff65748a));
            g.drawText ("3D VIEW", rect3D, juce::Justification::centred);
        }

        // 2D VIEW Button
        if (! is3DMode)
        {
            g.setColour (juce::Colour (0xff122336));
            g.fillRoundedRectangle (rect2D, rect2D.getHeight() * 0.5f);
            g.setColour (juce::Colour (0xff24d7ff));
            g.drawRoundedRectangle (rect2D, rect2D.getHeight() * 0.5f, 1.0f);
            g.setColour (juce::Colour (0xff24d7ff).withAlpha (0.25f));
            g.drawRoundedRectangle (rect2D.expanded (1.0f), rect2D.getHeight() * 0.5f, 2.0f);
            g.setFont (juce::FontOptions ("Segoe UI", 8.5f, juce::Font::bold));
            g.setColour (juce::Colour (0xfff2f6fc));
            g.drawText ("2D VIEW", rect2D, juce::Justification::centred);
        }
        else
        {
            g.setColour (juce::Colour (0xff0a111b));
            g.fillRoundedRectangle (rect2D, rect2D.getHeight() * 0.5f);
            g.setColour (juce::Colour (0xff243246));
            g.drawRoundedRectangle (rect2D, rect2D.getHeight() * 0.5f, 1.0f);
            g.setFont (juce::FontOptions ("Segoe UI", 8.5f, juce::Font::bold));
            g.setColour (juce::Colour (0xff65748a));
            g.drawText ("2D VIEW", rect2D, juce::Justification::centred);
        }
    }

    juce::Rectangle<float> get3DViewButtonBounds (juce::Rectangle<float> bounds) const
    {
        return juce::Rectangle<float> (bounds.getRight() - 118.0f, bounds.getY() + 8.0f, 52.0f, 18.0f);
    }

    juce::Rectangle<float> get2DViewButtonBounds (juce::Rectangle<float> bounds) const
    {
        return juce::Rectangle<float> (bounds.getRight() - 60.0f, bounds.getY() + 8.0f, 50.0f, 18.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Check 3D / 2D View Buttons
        if (get3DViewButtonBounds (bounds).contains (e.position))
        {
            is3DMode = true;
            repaint();
            return;
        }
        if (get2DViewButtonBounds (bounds).contains (e.position))
        {
            is3DMode = false;
            repaint();
            return;
        }

        // 2. Check Bottom Space Slider Bar Click / Drag
        auto sliderBarRect = getSpaceSliderBarBounds (bounds);
        if (sliderBarRect.expanded (6.0f, 8.0f).contains (e.position))
        {
            isDraggingSpaceSlider = true;
            updateSpaceSliderFromMouse (e.position.x, sliderBarRect);
            return;
        }

        // 3. Check Central Puck Drag
        auto geom = getGeometry (bounds);
        float distToPuck = geom.puckPos.getDistanceFrom (e.position);
        float distToCenter = geom.centre.getDistanceFrom (e.position);

        if (distToPuck <= 36.0f || distToCenter <= geom.radius * 1.05f)
        {
            isDraggingPuck = true;
            updatePuckFromMouse (e.position, geom.vBright, geom.vWarm, geom.vSpacious);
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto bounds = getLocalBounds().toFloat();

        if (isDraggingSpaceSlider)
        {
            auto sliderBarRect = getSpaceSliderBarBounds (bounds);
            updateSpaceSliderFromMouse (e.position.x, sliderBarRect);
            return;
        }

        if (isDraggingPuck)
        {
            auto geom = getGeometry (bounds);
            updatePuckFromMouse (e.position, geom.vBright, geom.vWarm, geom.vSpacious);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        isDraggingPuck = false;
        isDraggingSpaceSlider = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto sliderBarRect = getSpaceSliderBarBounds (bounds).expanded (6.0f, 8.0f);

        if (sliderBarRect.contains (e.position))
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        else if (get3DViewButtonBounds (bounds).contains (e.position) || get2DViewButtonBounds (bounds).contains (e.position))
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

private:
    float rmsIn = 0.0f;
    float rmsOut = 0.0f;
    float spaceSize = 70.0f;
    float decayTime = 2.8f;
    float diffusion = 75.0f;
    float damping = 45.0f;
    float predelay = 20.0f;
    float shimmer = 0.0f;
    float phaseCorr = 1.0f;
    float lowCut = 120.0f;
    float highCut = 12000.0f;
    float ducking = 0.0f;

    bool is3DMode { true };

    float weightBright { 0.333f };
    float weightWarm   { 0.333f };
    float weightSpace  { 0.334f };

    float rotationAngle = 0.0f;
    float shimmerPhase = 0.0f;
    float contourPulsePhase = 0.0f;
    float colorMorphPhase = 0.0f;

    bool isDraggingPuck = false;
    bool isDraggingSpaceSlider = false;

    bool isAnalyzing = false;
    float analyzeProgress = 0.0f;

    juce::Point<float> getVertexPosition (juce::Point<float> centre, float radius, float scaleY, int index) const
    {
        float angle = 0.0f;
        if (index == 0) // Top (BRIGHT)
            angle = -juce::MathConstants<float>::halfPi;
        else if (index == 1) // Bottom-Left (WARM - 150 deg)
            angle = juce::MathConstants<float>::halfPi + juce::MathConstants<float>::pi / 3.0f;
        else // Bottom-Right (SPACIOUS - 30 deg)
            angle = juce::MathConstants<float>::halfPi - juce::MathConstants<float>::pi / 3.0f;

        return juce::Point<float> (
            centre.x + std::cos (angle) * radius,
            centre.y + std::sin (angle) * radius * scaleY
        );
    }

    juce::Rectangle<float> getSpaceSliderBarBounds (juce::Rectangle<float> bounds) const
    {
        float barW = juce::jlimit (120.0f, 260.0f, bounds.getWidth() * 0.38f);
        float barH = 5.0f;
        float barX = bounds.getX() + 92.0f;
        float barY = bounds.getBottom() - 17.0f;
        return juce::Rectangle<float> (barX, barY, barW, barH);
    }

    void updateSpaceSliderFromMouse (float mouseX, juce::Rectangle<float> barRect)
    {
        float norm = juce::jlimit (0.0f, 1.0f, (mouseX - barRect.getX()) / barRect.getWidth());
        spaceSize = 10.0f + norm * 90.0f;
        if (onSpaceSizeChanged)
            onSpaceSizeChanged (spaceSize);
        repaint();
    }

    void updatePuckFromMouse (juce::Point<float> mousePos,
                              juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2)
    {
        float denom = (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y);
        if (std::abs (denom) < 0.0001f)
            return;

        float w0 = ((v1.y - v2.y) * (mousePos.x - v2.x) + (v2.x - v1.x) * (mousePos.y - v2.y)) / denom;
        float w1 = ((v2.y - v0.y) * (mousePos.x - v2.x) + (v0.x - v2.x) * (mousePos.y - v2.y)) / denom;
        float w2 = 1.0f - w0 - w1;

        w0 = juce::jmax (0.02f, w0);
        w1 = juce::jmax (0.02f, w1);
        w2 = juce::jmax (0.02f, w2);

        float sum = w0 + w1 + w2;
        weightBright = w0 / sum;
        weightWarm   = w1 / sum;
        weightSpace  = w2 / sum;

        if (onAcousticBalanceChanged)
            onAcousticBalanceChanged (weightBright, weightWarm, weightSpace);

        repaint();
    }

    void drawAtmosphericNebula (juce::Graphics& g, juce::Point<float> centre, float radius,
                                juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2,
                                juce::Point<float> puckPos,
                                juce::Colour colBright, juce::Colour colWarm,
                                juce::Colour colSpacious, juce::Colour colAmber)
    {
        const float audioPulse = rmsOut * 0.30f;
        const float r = radius * (1.0f + audioPulse * 0.06f);

        auto midBottom = (v1 + v2) * 0.5f;
        auto midLeft   = (v0 + v1) * 0.5f;
        auto midRight  = (v0 + v2) * 0.5f;

        const float driftX = std::sin (colorMorphPhase) * (r * 0.025f);
        const float driftY = std::cos (colorMorphPhase * 0.85f) * (r * 0.025f);

        // Exterior soft clouds
        float leftGlowX = centre.x - r * 0.85f + driftX;
        float leftGlowY = centre.y - r * 0.05f + driftY;
        juce::ColourGradient sideLeftGlow (colWarm.withAlpha (0.35f), leftGlowX, leftGlowY,
                                           colWarm.withAlpha (0.0f), leftGlowX - r * 0.90f, leftGlowY, true);
        g.setGradientFill (sideLeftGlow);
        g.fillEllipse (leftGlowX - r * 0.90f, leftGlowY - r * 0.90f, r * 1.80f, r * 1.80f);

        float rightGlowX = centre.x + r * 0.85f - driftX;
        float rightGlowY = centre.y - r * 0.05f + driftY;
        juce::ColourGradient sideRightGlow (colSpacious.withAlpha (0.35f), rightGlowX, rightGlowY,
                                            colSpacious.withAlpha (0.0f), rightGlowX + r * 0.90f, rightGlowY, true);
        g.setGradientFill (sideRightGlow);
        g.fillEllipse (rightGlowX - r * 0.90f, rightGlowY - r * 0.90f, r * 1.80f, r * 1.80f);

        float topGlowX = centre.x + driftX * 0.5f;
        float topGlowY = v0.y - r * 0.08f + driftY * 0.5f;
        juce::ColourGradient topGlow (colBright.withAlpha (0.35f), topGlowX, topGlowY,
                                      colBright.withAlpha (0.0f), topGlowX, topGlowY + r * 0.85f, true);
        g.setGradientFill (topGlow);
        g.fillEllipse (topGlowX - r * 0.85f, topGlowY - r * 0.60f, r * 1.70f, r * 1.40f);

        // Rounded Triangle Path
        juce::Path trianglePath;
        buildRoundedTrianglePath (trianglePath, v0, v1, v2, 24.0f);

        // Base glass fill
        g.setColour (juce::Colour (0xf0070b12));
        g.fillPath (trianglePath);

        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (trianglePath);

        const float duckFactor = juce::jlimit (0.35f, 1.0f, 1.0f - (ducking / 100.0f) * (rmsIn * 2.5f));
        const float warmGain = juce::jlimit (0.25f, 1.15f, 1.05f - ((lowCut - 20.0f) / 980.0f) * 0.45f);
        const float brightGain = juce::jlimit (0.30f, 1.25f, 0.4f + ((highCut - 1000.0f) / 19000.0f) * 0.85f);

        // Top Apex Electric Sky Cyan
        float cyanAlpha = juce::jlimit (0.25f, 0.82f, (0.65f + weightBright * 0.18f) * brightGain * duckFactor);
        juce::ColourGradient gradCyan (colBright.withAlpha (cyanAlpha), v0.x + driftX, v0.y + driftY,
                                       colBright.withAlpha (0.0f), v0.x, centre.y + r * 0.25f, true);
        g.setGradientFill (gradCyan);
        g.fillEllipse (v0.x - r * 0.90f, v0.y - r * 0.45f, r * 1.80f, r * 1.50f);

        // Bottom-Left Warm Rose / Magenta
        float warmAlpha = juce::jlimit (0.25f, 0.80f, (0.62f + weightWarm * 0.18f) * warmGain * duckFactor);
        juce::ColourGradient gradWarm (colWarm.withAlpha (warmAlpha), v1.x - driftX, v1.y + driftY,
                                       colWarm.withAlpha (0.0f), centre.x + r * 0.15f, centre.y - r * 0.15f, true);
        g.setGradientFill (gradWarm);
        g.fillEllipse (v1.x - r * 0.90f, v1.y - r * 0.80f, r * 1.80f, r * 1.60f);

        // Bottom-Right Mint Green
        float spaceAlpha = juce::jlimit (0.25f, 0.80f, (0.62f + weightSpace * 0.18f) * duckFactor);
        juce::ColourGradient gradSpace (colSpacious.withAlpha (spaceAlpha), v2.x + driftX, v2.y - driftY,
                                        colSpacious.withAlpha (0.0f), centre.x - r * 0.15f, centre.y - r * 0.15f, true);
        g.setGradientFill (gradSpace);
        g.fillEllipse (v2.x - r * 0.90f, v2.y - r * 0.80f, r * 1.80f, r * 1.60f);

        // Left Slope Soft Violet
        auto colViolet = juce::Colour (0xff9460ff);
        float violetAlpha = (0.50f + (weightBright + weightWarm) * 0.16f) * duckFactor;
        juce::ColourGradient gradViolet (colViolet.withAlpha (violetAlpha), midLeft.x, midLeft.y,
                                         colViolet.withAlpha (0.0f), centre.x, centre.y, true);
        g.setGradientFill (gradViolet);
        g.fillEllipse (midLeft.x - r * 0.60f, midLeft.y - r * 0.60f, r * 1.20f, r * 1.20f);

        // Bottom Center Golden-Amber
        float amberAlpha = (0.55f + (weightWarm + weightSpace) * 0.14f) * duckFactor;
        juce::ColourGradient bottomAmber (colAmber.withAlpha (amberAlpha), midBottom.x, midBottom.y + 4.0f,
                                          colAmber.withAlpha (0.0f), midBottom.x, midBottom.y - r * 0.45f, true);
        g.setGradientFill (bottomAmber);
        g.fillEllipse (midBottom.x - r * 0.70f, midBottom.y - r * 0.35f, r * 1.40f, r * 0.80f);

        // Center Opal Core Bloom around puck
        float coreAlpha = (0.35f + audioPulse * 0.35f) * duckFactor;
        juce::ColourGradient gradCore (juce::Colour (0x70ffffff).withAlpha (coreAlpha), puckPos.x, puckPos.y,
                                       juce::Colour (0x00ffffff), puckPos.x, puckPos.y + r * 0.38f, true);
        g.setGradientFill (gradCore);
        g.fillEllipse (puckPos.x - r * 0.38f, puckPos.y - r * 0.38f, r * 0.76f, r * 0.76f);
    }

    void drawLayeredGhostHulls (juce::Graphics& g, juce::Point<float> centre,
                                juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2,
                                juce::Colour colBright, juce::Colour colWarm, juce::Colour colSpacious)
    {
        auto expandVertex = [&centre] (juce::Point<float> v, float factor) {
            return centre + (v - centre) * factor;
        };

        const float hullScales[3]   = { 1.08f, 1.18f, 1.28f };
        const float strokeAlphas[3] = { 0.28f, 0.16f, 0.08f };
        const float fillAlphas[3]   = { 0.040f, 0.020f, 0.010f };

        for (int i = 0; i < 3; ++i)
        {
            juce::Path hull;
            buildRoundedTrianglePath (hull,
                                      expandVertex (v0, hullScales[i]),
                                      expandVertex (v1, hullScales[i]),
                                      expandVertex (v2, hullScales[i]),
                                      24.0f + (float) i * 2.5f);

            juce::ColourGradient hullGrad (colBright.withAlpha (fillAlphas[i] * 1.2f), centre.x, centre.y - 40.0f,
                                           colWarm.withAlpha (fillAlphas[i]), centre.x - 40.0f, centre.y + 40.0f, false);
            hullGrad.addColour (0.7, colSpacious.withAlpha (fillAlphas[i]));
            g.setGradientFill (hullGrad);
            g.fillPath (hull);

            g.setColour (colBright.withAlpha (strokeAlphas[i]));
            g.strokePath (hull, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void drawOrbitalRings (juce::Graphics& g, juce::Point<float> centre, float radius, float scaleY,
                           juce::Colour colBright, juce::Colour colWarm, juce::Colour colSpacious, juce::Colour colAmber)
    {
        const float r1 = radius * 0.28f;
        const float r2 = radius * 0.55f;
        const float r3 = radius * 0.78f;
        const float r4 = radius * 1.00f;
        const float r5 = radius * 1.24f;

        auto drawOrbit = [&](float r, juce::Colour col, float thickness)
        {
            g.setColour (col);
            g.drawEllipse (centre.x - r, centre.y - r * scaleY, r * 2.0f, r * 2.0f * scaleY, thickness);
        };

        drawOrbit (r1, juce::Colour (0x1224d7ff), 1.0f);
        drawOrbit (r2, juce::Colour (0x1424d7ff), 1.0f);
        drawOrbit (r3, juce::Colour (0x1624d7ff), 1.0f);
        drawOrbit (r4, juce::Colour (0x2424d7ff), 1.1f);

        // Outer Orbit with soft bloom
        drawOrbit (r5, juce::Colour (0x1224d7ff), 4.0f);
        drawOrbit (r5, juce::Colour (0x2c24d7ff), 1.2f);

        // Side Arcs (Left: Magenta, Right: Mint)
        juce::Path leftArc;
        leftArc.addCentredArc (centre.x, centre.y, r5, r5 * scaleY, 0.0f,
                               juce::MathConstants<float>::pi * 0.55f,
                               juce::MathConstants<float>::pi * 1.35f, true);
        g.setColour (colWarm.withAlpha (0.42f));
        g.strokePath (leftArc, juce::PathStrokeType (3.0f));
        g.setColour (colWarm.withAlpha (0.75f));
        g.strokePath (leftArc, juce::PathStrokeType (1.2f));

        juce::Path rightArc;
        rightArc.addCentredArc (centre.x, centre.y, r5, r5 * scaleY, 0.0f,
                                -juce::MathConstants<float>::pi * 0.35f,
                                juce::MathConstants<float>::pi * 0.45f, true);
        g.setColour (colSpacious.withAlpha (0.42f));
        g.strokePath (rightArc, juce::PathStrokeType (3.0f));
        g.setColour (colSpacious.withAlpha (0.75f));
        g.strokePath (rightArc, juce::PathStrokeType (1.2f));

        // Satellites on outer orbit
        float a1 = rotationAngle + 2.82f;
        float x1 = centre.x + std::cos (a1) * r5;
        float y1 = centre.y + std::sin (a1) * r5 * scaleY;
        g.setColour (colWarm.withAlpha (0.45f));
        g.fillEllipse (x1 - 10.0f, y1 - 10.0f, 20.0f, 20.0f);
        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (x1 - 2.5f, y1 - 2.5f, 5.0f, 5.0f);

        float a2 = rotationAngle + 0.42f;
        float x2 = centre.x + std::cos (a2) * r5;
        float y2 = centre.y + std::sin (a2) * r5 * scaleY;
        g.setColour (colSpacious.withAlpha (0.45f));
        g.fillEllipse (x2 - 10.0f, y2 - 10.0f, 20.0f, 20.0f);
        g.setColour (colBright);
        g.fillEllipse (x2 - 2.5f, y2 - 2.5f, 5.0f, 5.0f);

        float a3 = rotationAngle + 4.90f;
        float x3 = centre.x + std::cos (a3) * r5;
        float y3 = centre.y + std::sin (a3) * r5 * scaleY;
        g.setColour (colAmber.withAlpha (0.40f));
        g.fillEllipse (x3 - 8.0f, y3 - 8.0f, 16.0f, 16.0f);
        g.setColour (colAmber);
        g.fillEllipse (x3 - 2.0f, y3 - 2.0f, 4.0f, 4.0f);
    }

    void drawResonanceContours (juce::Graphics& g, juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2,
                                juce::Point<float> puckPos, float radius)
    {
        juce::ignoreUnused (puckPos);
        juce::Path trianglePath;
        buildRoundedTrianglePath (trianglePath, v0, v1, v2, 24.0f);

        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (trianglePath);

        const int numWaves = 16;
        const float topY = v0.y + 10.0f;
        const float botY = std::max (v1.y, v2.y) - 6.0f;
        const float stepY = (botY - topY) / (float) (numWaves + 1);

        for (int i = 0; i < numWaves; ++i)
        {
            float y = topY + (float) (i + 1) * stepY;
            float normY = (float) (i + 1) / (float) (numWaves + 1);

            juce::Path wavePath;
            float startX = v0.x - radius * 1.15f;
            float endX   = v0.x + radius * 1.15f;
            const int numSegments = 32;
            float stepX = (endX - startX) / (float) numSegments;

            float phase = contourPulsePhase + normY * juce::MathConstants<float>::pi * 1.5f + colorMorphPhase * 0.45f;

            for (int s = 0; s <= numSegments; ++s)
            {
                float x = startX + (float) s * stepX;
                float distFromCenter = std::abs (x - v0.x) / (radius * 1.15f);
                float env = std::cos (juce::jmin (1.0f, distFromCenter) * juce::MathConstants<float>::halfPi);

                float wave = std::sin (phase + (float) s * 0.35f) * 3.5f * env * (1.0f + rmsOut * 0.4f);
                float ptY = y + wave;

                if (s == 0)
                    wavePath.startNewSubPath (x, ptY);
                else
                    wavePath.lineTo (x, ptY);
            }

            float alpha = (0.10f + rmsOut * 0.16f) * (0.8f + 0.2f * std::sin (contourPulsePhase + (float) i * 0.4f));
            g.setColour (juce::Colour (0xffffffff).withAlpha (alpha));
            g.strokePath (wavePath, juce::PathStrokeType (1.0f));
        }
    }

    void drawAcousticShell (juce::Graphics& g, juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2)
    {
        juce::Path shellPath;
        buildRoundedTrianglePath (shellPath, v0, v1, v2, 24.0f);

        g.setColour (juce::Colour (0x1524d7ff));
        g.strokePath (shellPath, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (0x3524d7ff));
        g.strokePath (shellPath, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (0x8bf2f6fc));
        g.strokePath (shellPath, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void buildRoundedTrianglePath (juce::Path& p, juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2, float cornerRadius) const
    {
        p.clear();
        juce::ignoreUnused (cornerRadius);
        const auto c = (v0 + v1 + v2) / 3.0f;
        const juce::Point<float> pts[3] = { v0, v1, v2 };

        const float cut = 0.22f;

        juce::Point<float> startPt[3];
        juce::Point<float> endPt[3];

        for (int i = 0; i < 3; ++i)
        {
            int prev = (i + 2) % 3;
            int next = (i + 1) % 3;

            endPt[i]   = pts[i] + (pts[prev] - pts[i]) * cut;
            startPt[i] = pts[i] + (pts[next] - pts[i]) * cut;
        }

        p.startNewSubPath (startPt[0]);

        for (int i = 0; i < 3; ++i)
        {
            int next = (i + 1) % 3;

            auto midStraight = (startPt[i] + endPt[next]) * 0.5f;
            auto outward = midStraight - c;
            auto ctrlPt = midStraight + outward * 0.22f;

            p.quadraticTo (ctrlPt.x, ctrlPt.y, endPt[next].x, endPt[next].y);
            p.quadraticTo (pts[next].x, pts[next].y, startPt[next].x, startPt[next].y);
        }

        p.closeSubPath();
    }

    void drawShimmerParticles (juce::Graphics& g, juce::Point<float> centre, float radius, float scaleY)
    {
        int numParticles = (int) (shimmer * 0.15f);
        for (int i = 0; i < numParticles; ++i)
        {
            float pAngle = ((float) i * 1.618f) + shimmerPhase;
            float pDist  = (std::sin ((float) i + shimmerPhase * 0.8f) * 0.5f + 0.5f) * radius * 0.65f;

            float px = centre.x + std::cos (pAngle) * pDist;
            float py = centre.y + std::sin (pAngle) * pDist * scaleY;
            float pAlpha = (std::sin (shimmerPhase * 2.5f + (float) i) * 0.5f + 0.5f) * (shimmer / 100.0f);

            juce::Colour sparkCol = (i % 2 == 0) ? juce::Colour (0xff24d7ff) : juce::Colour (0xffffb451);
            g.setColour (sparkCol.withAlpha (pAlpha));
            g.fillEllipse (px - 1.5f, py - 1.5f, 3.0f, 3.0f);
        }
    }

    void drawVertexNode (juce::Graphics& g, juce::Point<float> pt, const juce::String& label, juce::Colour col, float weight)
    {
        const float nodeR = 7.5f;

        g.setColour (col.withAlpha (0.35f + weight * 0.35f));
        g.fillEllipse (pt.x - 14.0f, pt.y - 14.0f, 28.0f, 28.0f);

        g.setColour (juce::Colour (0xff070b12));
        g.fillEllipse (pt.x - nodeR, pt.y - nodeR, nodeR * 2.0f, nodeR * 2.0f);

        g.setColour (col);
        g.drawEllipse (pt.x - nodeR, pt.y - nodeR, nodeR * 2.0f, nodeR * 2.0f, 2.0f);

        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (pt.x - 2.2f, pt.y - 2.2f, 4.4f, 4.4f);

        g.setFont (juce::FontOptions ("Segoe UI", 9.5f, juce::Font::bold));
        g.setColour (juce::Colour (0xfff2f6fc));

        if (label == "BRIGHT")
            g.drawText (label, (int) (pt.x - 45.0f), (int) (pt.y - nodeR - 16.0f), 90, 15, juce::Justification::centred);
        else
            g.drawText (label, (int) (pt.x - 45.0f), (int) (pt.y + nodeR + 3.0f), 90, 15, juce::Justification::centred);
    }

    void drawControlPuck (juce::Graphics& g, juce::Point<float> pos)
    {
        const float puckR = 14.0f + (rmsOut * 2.5f);

        g.setColour (juce::Colour (0x55000000));
        g.fillEllipse (pos.x - puckR - 3.5f, pos.y - puckR - 3.5f, (puckR + 3.5f) * 2.0f, (puckR + 3.5f) * 2.0f);

        g.setColour (juce::Colour (0x28ffffff));
        g.fillEllipse (pos.x - puckR - 2.0f, pos.y - puckR - 2.0f, (puckR + 2.0f) * 2.0f, (puckR + 2.0f) * 2.0f);

        g.setColour (juce::Colour (0x30ffffff));
        g.fillEllipse (pos.x - puckR, pos.y - puckR, puckR * 2.0f, puckR * 2.0f);

        g.setColour (juce::Colour (0xffffffff));
        g.drawEllipse (pos.x - puckR, pos.y - puckR, puckR * 2.0f, puckR * 2.0f, 2.8f);
    }

    void drawBottomBar (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        float bottomY = bounds.getBottom() - 22.0f;

        // SPACE: XX% Label
        g.setFont (juce::FontOptions ("Segoe UI", 9.5f, juce::Font::bold));
        g.setColour (juce::Colour (0xff24d7ff));
        juce::String spaceText = "SPACE: " + juce::String ((int) spaceSize) + "%";
        g.drawText (spaceText, (int) bounds.getX() + 14, (int) bottomY, 74, 16, juce::Justification::centredLeft);

        // Rainbow Gradient Slider Bar
        auto barRect = getSpaceSliderBarBounds (bounds);
        g.setColour (juce::Colour (0xff0a111b));
        g.fillRoundedRectangle (barRect, 2.5f);

        juce::ColourGradient rainbowGrad (juce::Colour (0xff24d7ff), barRect.getX(), barRect.getY(),
                                          juce::Colour (0xff35e6b5), barRect.getRight(), barRect.getY(), false);
        rainbowGrad.addColour (0.28, juce::Colour (0xff9460ff));
        rainbowGrad.addColour (0.55, juce::Colour (0xfff34db7));
        rainbowGrad.addColour (0.78, juce::Colour (0xffffb451));
        g.setGradientFill (rainbowGrad);

        float fillNorm = (spaceSize - 10.0f) / 90.0f;
        auto fillRect = barRect.withWidth (barRect.getWidth() * fillNorm);
        g.fillRoundedRectangle (fillRect, 2.5f);

        // Draggable thumb indicator
        float thumbX = barRect.getX() + barRect.getWidth() * fillNorm;
        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (thumbX - 4.5f, barRect.getCentreY() - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colour (0xff24d7ff));
        g.drawEllipse (thumbX - 4.5f, barRect.getCentreY() - 4.5f, 9.0f, 9.0f, 1.2f);

        // Real-Time Stereo Phase Correlation Meter
        const int meterW = 48;
        const int meterH = 5;
        const int meterX = (int) bounds.getRight() - meterW - 14;
        const int meterY = (int) bounds.getBottom() - 17;

        g.setColour (juce::Colour (0xff0a111b));
        g.fillRoundedRectangle ((float) meterX, (float) meterY, (float) meterW, (float) meterH, 2.0f);
        g.setColour (juce::Colour (0xff243246));
        g.drawRoundedRectangle ((float) meterX, (float) meterY, (float) meterW, (float) meterH, 2.0f, 0.8f);

        g.setColour (juce::Colour (0x66ffffff));
        g.drawVerticalLine (meterX + meterW / 2, (float) meterY, (float) (meterY + meterH));

        float pinNorm = juce::jlimit (0.0f, 1.0f, (phaseCorr + 1.0f) * 0.5f);
        float pinX = meterX + pinNorm * (float) (meterW - 4);
        juce::Colour phaseCol = (phaseCorr > 0.35f) ? juce::Colour (0xff35e6b5)
                              : (phaseCorr >= 0.0f) ? juce::Colour (0xffffb451)
                              : juce::Colour (0xffff4d6d);

        g.setColour (phaseCol);
        g.fillRoundedRectangle (pinX, (float) meterY - 1.0f, 3.5f, (float) (meterH + 2), 1.5f);

        juce::String phaseText = juce::String ("PHASE: ") + (phaseCorr >= 0.0f ? "+" : "") + juce::String (phaseCorr, 2) + " |";
        g.setFont (juce::FontOptions ("Segoe UI", 9.0f, juce::Font::bold));
        g.setColour (phaseCol.withAlpha (0.95f));
        g.drawText (phaseText, meterX - 86, (int) bounds.getBottom() - 22, 82, 16, juce::Justification::right);
    }

    void drawAnalysisOverlay (juce::Graphics& g, juce::Point<float> centre, float radius)
    {
        float scanY = centre.y - radius * 0.8f + (radius * 1.6f * analyzeProgress);

        juce::ColourGradient beamGrad (juce::Colour (0x00ffffff), centre.x - radius, scanY,
                                       juce::Colour (0xffffffff), centre.x, scanY, true);
        g.setGradientFill (beamGrad);
        g.fillRect (centre.x - radius, scanY - 2.0f, radius * 2.0f, 4.0f);

        float pulseRadius = radius * (0.25f + 0.75f * std::fmod (analyzeProgress * 3.0f, 1.0f));
        g.setColour (juce::Colour (0x44ffffff));
        g.drawEllipse (centre.x - pulseRadius, centre.y - pulseRadius * 0.6f, pulseRadius * 2.0f, pulseRadius * 1.2f, 1.5f);

        g.setFont (juce::FontOptions ("Segoe UI", 10.5f, juce::Font::bold));
        g.setColour (juce::Colour (0xffffffff));
        g.drawText ("AI ACOUSTIC SCAN: " + juce::String ((int)(analyzeProgress * 100.0f)) + "%",
                    (int) centre.x - 100, (int) centre.y - 10, 200, 20, juce::Justification::centred);
    }
};
