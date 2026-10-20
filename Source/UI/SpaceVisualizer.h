#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>

/**
 * SpaceVisualizer — Next-Gen Interactive 3D Acoustic Space Visualizer
 * ProVerb by ProducerToy
 *
 * Replaces the old wireframe cylinder with a soft-edged, 3D rounded-triangular
 * acoustic field with multi-gradient rainbow atmospheric aura, interactive
 * BRIGHT / WARM / SPACIOUS barycentric control puck, concentric acoustic
 * resonance contours, 2D/3D perspective toggle, rainbow Space slider, and
 * real-time DSP stereo phase correlation indicator.
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

        // Smoothly synchronize puck position with parameter values when user is not manually dragging it
        if (! isDraggingPuck)
        {
            // Derive balance from actual parameters:
            // High Cut & low Damping => BRIGHT
            // High Damping & low Low Cut => WARM
            // High Size, Diffusion, Width => SPACIOUS
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

            // Smooth lowpass towards target
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
        // Smooth rotation speed for orbital markers and wave ripples
        rotationAngle += deltaPhase * 0.4f;
        if (rotationAngle > juce::MathConstants<float>::twoPi)
            rotationAngle -= juce::MathConstants<float>::twoPi;

        // Sparkle animation for shimmer particles
        shimmerPhase += deltaPhase * 1.6f;
        if (shimmerPhase > juce::MathConstants<float>::twoPi)
            shimmerPhase -= juce::MathConstants<float>::twoPi;

        // Acoustic contour breathing wave
        contourPulsePhase += deltaPhase * 1.8f;
        if (contourPulsePhase > juce::MathConstants<float>::twoPi)
            contourPulsePhase -= juce::MathConstants<float>::twoPi;

        repaint();
    }    struct Geometry
    {
        juce::Point<float> centre;
        float radius { 100.0f };
        float scaleY { 0.96f };
        juce::Point<float> vBright;
        juce::Point<float> vWarm;
        juce::Point<float> vSpacious;
        juce::Point<float> puckPos;
    };

    Geometry getGeometry (juce::Rectangle<float> bounds) const
    {
        Geometry geom;
        geom.scaleY = 1.0f; // True equilateral circular proportions

        // Position center with clean vertical balance
        geom.centre = juce::Point<float> (bounds.getCentreX(), bounds.getY() + bounds.getHeight() * 0.45f);

        // Balanced radius: prominent, commanding, with elegant breathing room for labels
        float minDim = juce::jmin (bounds.getWidth(), bounds.getHeight());
        geom.radius = minDim * 0.36f;

        geom.vBright   = getVertexPosition (geom.centre, geom.radius, geom.scaleY, 0);
        geom.vWarm     = getVertexPosition (geom.centre, geom.radius, geom.scaleY, 1);
        geom.vSpacious = getVertexPosition (geom.centre, geom.radius, geom.scaleY, 2);

        geom.puckPos = juce::Point<float> (
            weightBright * geom.vBright.x + weightWarm * geom.vWarm.x + weightSpace * geom.vSpacious.x,
            weightBright * geom.vBright.y + weightWarm * geom.vWarm.y + weightSpace * geom.vSpacious.y
        );

        return geom;
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Futuristic Deep Obsidian Background (Seamless full-bleed card canvas)
        g.setColour (juce::Colour (0xff07080c));
        g.fillRoundedRectangle (bounds, 6.0f);

        auto geom = getGeometry (bounds);

        // 2. Draw Concentric Circular Radar Grid & Satellite Nodes
        drawOrbitalRings (g, geom.centre, geom.radius);

        // 3. Draw Ambient Colored Atmospheric Glows in Background
        drawAtmosphericNebula (g, geom.centre, geom.radius, geom.vBright, geom.vWarm, geom.vSpacious, geom.puckPos);

        // 4. Draw Organic Acoustic Topographic Wavefront Ripples
        drawResonanceContours (g, geom.vBright, geom.vWarm, geom.vSpacious, geom.puckPos, geom.radius);

        // 5. Draw Multi-Pass Luminous Rim (Smooth Convex Rounded-Triangle Shell)
        drawAcousticShell (g, geom.vBright, geom.vWarm, geom.vSpacious);

        // 6. Draw Shimmer Floating Sparkling Particles
        if (shimmer > 1.0f)
        {
            drawShimmerParticles (g, geom.centre, geom.radius, geom.scaleY);
        }

        // 7. Draw Vertex Anchor Badges (BRIGHT, WARM, SPACIOUS)
        drawVertexNode (g, geom.vBright,   "BRIGHT",   juce::Colour (0xff00d4ff), weightBright);
        drawVertexNode (g, geom.vWarm,     "WARM",     juce::Colour (0xffff2a6d), weightWarm);
        drawVertexNode (g, geom.vSpacious, "SPACIOUS", juce::Colour (0xff00f59b), weightSpace);

        // 8. Draw Central Control Puck (Pure White Glowing Ring with Translucent Frosted Glass Fill)
        drawControlPuck (g, geom.puckPos);

        // 9. Draw Bottom Controls (SPACE Slider & Real-Time Phase Correlation Meter)
        drawBottomBar (g, bounds);

        // 10. Draw AI Acoustic Scan Overlay (if analyzing)
        if (isAnalyzing)
        {
            drawAnalysisOverlay (g, geom.centre, geom.radius);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Check Bottom Space Slider Bar Click / Drag
        auto sliderBarRect = getSpaceSliderBarBounds (bounds);
        if (sliderBarRect.expanded (4.0f, 6.0f).contains (e.position))
        {
            isDraggingSpaceSlider = true;
            updateSpaceSliderFromMouse (e.position.x, sliderBarRect);
            return;
        }

        // 2. Check Central Puck Drag
        auto geom = getGeometry (bounds);
        float distToPuck = geom.puckPos.getDistanceFrom (e.position);
        float distToCenter = geom.centre.getDistanceFrom (e.position);

        if (distToPuck <= 34.0f || distToCenter <= geom.radius * 1.05f)
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
        auto sliderBarRect = getSpaceSliderBarBounds (bounds).expanded (4.0f, 6.0f);

        if (sliderBarRect.contains (e.position))
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
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

    // Barycentric Balance Weights (Bright, Warm, Spacious)
    float weightBright { 0.333f };
    float weightWarm   { 0.333f };
    float weightSpace  { 0.334f };

    float rotationAngle = 0.0f;
    float shimmerPhase = 0.0f;
    float contourPulsePhase = 0.0f;

    bool isDraggingPuck = false;
    bool isDraggingSpaceSlider = false;

    bool isAnalyzing = false;
    float analyzeProgress = 0.0f;

    // Helper: calculate 3 triangle vertices (0 = Top Bright, 1 = Bottom-Left Warm, 2 = Bottom-Right Spacious)
    juce::Point<float> getVertexPosition (juce::Point<float> centre, float radius, float scaleY, int index) const
    {
        juce::ignoreUnused (scaleY);
        float angle = 0.0f;
        if (index == 0) // Top (BRIGHT)
            angle = -juce::MathConstants<float>::halfPi;
        else if (index == 1) // Bottom-Left (WARM - 150 deg)
            angle = juce::MathConstants<float>::halfPi + juce::MathConstants<float>::pi / 3.0f;
        else // Bottom-Right (SPACIOUS - 30 deg)
            angle = juce::MathConstants<float>::halfPi - juce::MathConstants<float>::pi / 3.0f;

        return juce::Point<float> (
            centre.x + std::cos (angle) * radius,
            centre.y + std::sin (angle) * radius
        );
    }

    juce::Rectangle<float> getSpaceSliderBarBounds (juce::Rectangle<float> bounds) const
    {
        float barW = 100.0f;
        float barH = 4.0f;
        float barX = bounds.getX() + 78.0f;
        float barY = bounds.getBottom() - 13.0f;
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
        // Compute barycentric coordinates for point mousePos relative to triangle (v0, v1, v2)
        float denom = (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y);
        if (std::abs (denom) < 0.0001f)
            return;

        float w0 = ((v1.y - v2.y) * (mousePos.x - v2.x) + (v2.x - v1.x) * (mousePos.y - v2.y)) / denom;
        float w1 = ((v2.y - v0.y) * (mousePos.x - v2.x) + (v0.x - v2.x) * (mousePos.y - v2.y)) / denom;
        float w2 = 1.0f - w0 - w1;

        // Clamp to positive space inside triangle
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

    // 2. Multi-gradient Atmospheric Aura Nebula (Soft Blurred Chromatic Rainbow Prism matching Neoverb 1:1)
    void drawAtmosphericNebula (juce::Graphics& g, juce::Point<float> centre, float radius,
                                juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2,
                                juce::Point<float> puckPos)
    {
        const float audioPulse = rmsOut * 0.35f;
        const float r = radius * (1.0f + audioPulse * 0.08f);

        auto midBottom = (v1 + v2) * 0.5f;
        auto midLeft   = (v0 + v1) * 0.5f;
        auto midRight  = (v0 + v2) * 0.5f;

        // 1. Exterior Atmospheric Colored Glows behind the triangle
        // Top Cyan Atmospheric Aura
        juce::ColourGradient bgCyan (juce::Colour (0x5500f0ff), v0.x, v0.y,
                                     juce::Colour (0x00000000), v0.x, v0.y + r * 0.85f, true);
        g.setGradientFill (bgCyan);
        g.fillEllipse (v0.x - r * 0.80f, v0.y - r * 0.60f, r * 1.60f, r * 1.40f);

        // Bottom-Left Magenta Atmospheric Aura
        juce::ColourGradient bgWarm (juce::Colour (0x45ff2a6d), v1.x, v1.y,
                                     juce::Colour (0x00000000), v1.x + r * 0.80f, v1.y - r * 0.35f, true);
        g.setGradientFill (bgWarm);
        g.fillEllipse (v1.x - r * 0.80f, v1.y - r * 0.70f, r * 1.60f, r * 1.40f);

        // Bottom-Right Emerald Atmospheric Aura
        juce::ColourGradient bgSpace (juce::Colour (0x4500f59b), v2.x, v2.y,
                                      juce::Colour (0x00000000), v2.x - r * 0.80f, v2.y - r * 0.35f, true);
        g.setGradientFill (bgSpace);
        g.fillEllipse (v2.x - r * 0.80f, v2.y - r * 0.70f, r * 1.60f, r * 1.40f);

        // Bottom Golden Smoke
        juce::ColourGradient bgAmber (juce::Colour (0x35fbbf24), centre.x, centre.y + r * 0.60f,
                                      juce::Colour (0x00000000), centre.x, centre.y + r * 0.10f, true);
        g.setGradientFill (bgAmber);
        g.fillEllipse (centre.x - r * 0.80f, centre.y + r * 0.20f, r * 1.60f, r * 0.85f);

        // 2. Build Convex Rounded Triangle Path
        juce::Path trianglePath;
        buildRoundedTrianglePath (trianglePath, v0, v1, v2, 22.0f);

        // 3. Fill with deep glass base
        g.setColour (juce::Colour (0xee090b12));
        g.fillPath (trianglePath);

        // 4. Clip rich multi-spectral liquid gradient inside the triangle
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (trianglePath);

        const float duckFactor = juce::jlimit (0.35f, 1.0f, 1.0f - (ducking / 100.0f) * (rmsIn * 2.5f));
        const float warmGain = juce::jlimit (0.25f, 1.15f, 1.05f - ((lowCut - 20.0f) / 980.0f) * 0.45f);
        const float brightGain = juce::jlimit (0.30f, 1.25f, 0.4f + ((highCut - 1000.0f) / 19000.0f) * 0.85f);
        const float shimmerBoost = (shimmer / 100.0f) * 0.25f;

        // Top Apex Electric Cyan (Reference match: solid glowing blue top third)
        float cyanAlpha = (0.85f + weightBright * 0.15f) * brightGain * duckFactor;
        juce::ColourGradient gradCyan (juce::Colour (0xff00f0ff).withAlpha (cyanAlpha), v0.x, v0.y,
                                       juce::Colour (0x000284c7), v0.x, centre.y + r * 0.25f, true);
        gradCyan.addColour (0.45, juce::Colour (0xdd00d4ff));
        g.setGradientFill (gradCyan);
        g.fillEllipse (v0.x - r * 0.90f, v0.y - r * 0.45f, r * 1.80f, r * 1.50f);

        // Bottom-Left Vivid Coral / Magenta
        float warmAlpha = (0.82f + weightWarm * 0.18f) * warmGain * duckFactor;
        juce::ColourGradient gradWarm (juce::Colour (0xffff2079).withAlpha (warmAlpha), v1.x, v1.y,
                                       juce::Colour (0x00c026d3), centre.x + r * 0.15f, centre.y - r * 0.15f, true);
        gradWarm.addColour (0.40, juce::Colour (0xddff2a6d));
        g.setGradientFill (gradWarm);
        g.fillEllipse (v1.x - r * 0.90f, v1.y - r * 0.80f, r * 1.80f, r * 1.60f);

        // Bottom-Right Vivid Emerald Mint
        float spaceAlpha = (0.82f + weightSpace * 0.18f) * duckFactor;
        juce::ColourGradient gradSpace (juce::Colour (0xff00f59b).withAlpha (spaceAlpha), v2.x, v2.y,
                                        juce::Colour (0x00059669), centre.x - r * 0.15f, centre.y - r * 0.15f, true);
        gradSpace.addColour (0.40, juce::Colour (0xdd10b981));
        g.setGradientFill (gradSpace);
        g.fillEllipse (v2.x - r * 0.90f, v2.y - r * 0.80f, r * 1.80f, r * 1.60f);

        // Left Slope Violet / Purple
        float violetAlpha = (0.70f + (weightBright + weightWarm) * 0.20f) * duckFactor;
        juce::ColourGradient gradViolet (juce::Colour (0xff8b5cf6).withAlpha (violetAlpha), midLeft.x, midLeft.y,
                                         juce::Colour (0x00000000), centre.x, centre.y, true);
        g.setGradientFill (gradViolet);
        g.fillEllipse (midLeft.x - r * 0.60f, midLeft.y - r * 0.60f, r * 1.20f, r * 1.20f);

        // Right Slope Ocean Teal
        float tealAlpha = (0.70f + (weightBright + weightSpace) * 0.20f + shimmerBoost) * duckFactor;
        juce::ColourGradient gradTeal (juce::Colour (0xff06b6d4).withAlpha (tealAlpha), midRight.x, midRight.y,
                                       juce::Colour (0x00000000), centre.x, centre.y, true);
        g.setGradientFill (gradTeal);
        g.fillEllipse (midRight.x - r * 0.60f, midRight.y - r * 0.60f, r * 1.20f, r * 1.20f);

        // Bottom Crescent Golden-Amber
        float amberAlpha = (0.78f + (weightWarm + weightSpace) * 0.12f) * duckFactor;
        juce::ColourGradient bottomAmber (juce::Colour (0xfffbbf24).withAlpha (amberAlpha), midBottom.x, midBottom.y + 4.0f,
                                          juce::Colour (0x00d97706), midBottom.x, midBottom.y - r * 0.45f, true);
        g.setGradientFill (bottomAmber);
        g.fillEllipse (midBottom.x - r * 0.70f, midBottom.y - r * 0.35f, r * 1.40f, r * 0.80f);

        // Center Luminous Milky Opal Core Bloom around puck
        float coreAlpha = (0.35f + audioPulse * 0.4f) * duckFactor;
        juce::ColourGradient gradCore (juce::Colour (0x80ffffff).withAlpha (coreAlpha), puckPos.x, puckPos.y,
                                       juce::Colour (0x00ffffff), puckPos.x, puckPos.y + r * 0.38f, true);
        g.setGradientFill (gradCore);
        g.fillEllipse (puckPos.x - r * 0.38f, puckPos.y - r * 0.38f, r * 0.76f, r * 0.76f);
    }

    // 3. Orbital Rings with Acoustic Satellites (1:1 with Reference)
    void drawOrbitalRings (juce::Graphics& g, juce::Point<float> centre, float radius)
    {
        // 5 Concentric Radar Circles
        const float r1 = radius * 0.28f; // Inner core orbit
        const float r2 = radius * 0.55f; // Mid-inner orbit
        const float r3 = radius * 0.78f; // Mid-outer orbit
        const float r4 = radius * 1.00f; // Vertex Orbit — PASSES PRECISELY THROUGH ALL 3 NODES
        const float r5 = radius * 1.22f; // Outer orbit with satellites

        g.setColour (juce::Colour (0x1838bdf8));
        g.drawEllipse (centre.x - r1, centre.y - r1, r1 * 2.0f, r1 * 2.0f, 1.0f);
        g.drawEllipse (centre.x - r2, centre.y - r2, r2 * 2.0f, r2 * 2.0f, 1.0f);
        g.drawEllipse (centre.x - r3, centre.y - r3, r3 * 2.0f, r3 * 2.0f, 1.0f);

        // Vertex Orbit (connecting the 3 nodes)
        g.setColour (juce::Colour (0x2838bdf8));
        g.drawEllipse (centre.x - r4, centre.y - r4, r4 * 2.0f, r4 * 2.0f, 1.1f);

        // Outer Orbit
        g.setColour (juce::Colour (0x1c38bdf8));
        g.drawEllipse (centre.x - r5, centre.y - r5, r5 * 2.0f, r5 * 2.0f, 1.0f);

        // Delicate Radar Azimuth Ticks at 30-degree intervals on r2 & r3
        for (int a = 0; a < 12; ++a)
        {
            float ang = (float) a * (juce::MathConstants<float>::twoPi / 12.0f);
            float cosA = std::cos (ang);
            float sinA = std::sin (ang);

            g.setColour (juce::Colour (0x1838bdf8));
            g.drawLine (centre.x + cosA * (r2 - 2.5f), centre.y + sinA * (r2 - 2.5f),
                        centre.x + cosA * (r2 + 2.5f), centre.y + sinA * (r2 + 2.5f), 1.0f);

            g.drawLine (centre.x + cosA * (r3 - 2.5f), centre.y + sinA * (r3 - 2.5f),
                        centre.x + cosA * (r3 + 2.5f), centre.y + sinA * (r3 + 2.5f), 1.0f);
        }

        // Orbiting Satellite Nodes on r5 (matching reference positions)
        // Satellite 1: Left orbit (~162 deg / 9:30 o'clock) — Pure White Glowing Dot
        float a1 = rotationAngle + 2.82f;
        float x1 = centre.x + std::cos (a1) * r5;
        float y1 = centre.y + std::sin (a1) * r5;
        g.setColour (juce::Colour (0x35ffffff));
        g.fillEllipse (x1 - 6.5f, y1 - 6.5f, 13.0f, 13.0f);
        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (x1 - 2.6f, y1 - 2.6f, 5.2f, 5.2f);

        // Satellite 2: Right orbit (~24 deg / 2:30 o'clock) — Pure Cyan Glowing Dot
        float a2 = rotationAngle + 0.42f;
        float x2 = centre.x + std::cos (a2) * r5;
        float y2 = centre.y + std::sin (a2) * r5;
        g.setColour (juce::Colour (0x3500f0ff));
        g.fillEllipse (x2 - 6.5f, y2 - 6.5f, 13.0f, 13.0f);
        g.setColour (juce::Colour (0xff00f0ff));
        g.fillEllipse (x2 - 2.6f, y2 - 2.6f, 5.2f, 5.2f);

        // Satellite 3: Bottom orbit (~280 deg / near 5:30 o'clock) — Faint Amber Speckle
        float a3 = rotationAngle + 4.90f;
        float x3 = centre.x + std::cos (a3) * r5;
        float y3 = centre.y + std::sin (a3) * r5;
        g.setColour (juce::Colour (0x30fbbf24));
        g.fillEllipse (x3 - 4.0f, y3 - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colour (0xfffbbf24));
        g.fillEllipse (x3 - 1.8f, y3 - 1.8f, 3.6f, 3.6f);
    }

    // 4. Concentric Acoustic Topographic Wavefront Ripples
    void drawResonanceContours (juce::Graphics& g, juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2,
                                juce::Point<float> puckPos, float radius)
    {
        juce::ignoreUnused (puckPos);
        juce::Path trianglePath;
        buildRoundedTrianglePath (trianglePath, v0, v1, v2, 22.0f);

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

            float phase = contourPulsePhase + normY * juce::MathConstants<float>::pi * 1.5f;

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

            float alpha = (0.12f + rmsOut * 0.16f) * (0.8f + 0.2f * std::sin (contourPulsePhase + (float) i * 0.4f));
            g.setColour (juce::Colour (0xffffffff).withAlpha (alpha));
            g.strokePath (wavePath, juce::PathStrokeType (1.0f));
        }
    }

    // 5. Draw Smooth Rounded-Triangular Acoustic Shell (Silky Luminous Rim Bloom — No harsh white outline!)
    void drawAcousticShell (juce::Graphics& g, juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2)
    {
        juce::Path shellPath;
        buildRoundedTrianglePath (shellPath, v0, v1, v2, 22.0f);

        // Soft atmospheric cyan bloom around shell
        g.setColour (juce::Colour (0x1800f0ff));
        g.strokePath (shellPath, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Crisp luminous rim highlight (matching reference organic fluid edge)
        g.setColour (juce::Colour (0x65e2e8f0));
        g.strokePath (shellPath, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Helper: construct a rounded triangle with curved convex edges (100% clean, non-self-intersecting)
    void buildRoundedTrianglePath (juce::Path& p, juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2, float cornerRadius) const
    {
        p.clear();
        juce::ignoreUnused (cornerRadius);
        const auto c = (v0 + v1 + v2) / 3.0f;
        const juce::Point<float> pts[3] = { v0, v1, v2 };

        // Corner cut fraction along edges
        const float cut = 0.22f;

        juce::Point<float> startPt[3]; // Point where rounding ends on outgoing edge
        juce::Point<float> endPt[3];   // Point where rounding begins on incoming edge

        for (int i = 0; i < 3; ++i)
        {
            int prev = (i + 2) % 3;
            int next = (i + 1) % 3;

            endPt[i]   = pts[i] + (pts[prev] - pts[i]) * cut;
            startPt[i] = pts[i] + (pts[next] - pts[i]) * cut;
        }

        // Start at startPt[0] (leaving vertex 0 towards vertex 1)
        p.startNewSubPath (startPt[0]);

        for (int i = 0; i < 3; ++i)
        {
            int next = (i + 1) % 3;

            // Side from startPt[i] to endPt[next] with gentle outward convex bulge
            auto midStraight = (startPt[i] + endPt[next]) * 0.5f;
            auto outward = midStraight - c;
            auto ctrlPt = midStraight + outward * 0.22f; // subtle, natural convex outward curve

            p.quadraticTo (ctrlPt.x, ctrlPt.y, endPt[next].x, endPt[next].y);

            // Round corner at vertex next: from endPt[next] around pts[next] to startPt[next]
            p.quadraticTo (pts[next].x, pts[next].y, startPt[next].x, startPt[next].y);
        }

        p.closeSubPath();
    }

    // 6. Draw Shimmer Floating Sparkling Particles
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

            juce::Colour sparkCol = (i % 2 == 0) ? juce::Colour (0xff00f0ff) : juce::Colour (0xfffef08a);
            g.setColour (sparkCol.withAlpha (pAlpha));
            g.fillEllipse (px - 1.5f, py - 1.5f, 3.0f, 3.0f);
        }
    }

    // 7. Draw Vertex Anchor Badges (BRIGHT, WARM, SPACIOUS) — 1:1 with Reference
    void drawVertexNode (juce::Graphics& g, juce::Point<float> pt, const juce::String& label, juce::Colour col, float weight)
    {
        const float nodeR = 7.5f;

        // Outer soft glow halo
        g.setColour (col.withAlpha (0.35f + weight * 0.35f));
        g.fillEllipse (pt.x - 14.0f, pt.y - 14.0f, 28.0f, 28.0f);

        // Dark socket ring base
        g.setColour (juce::Colour (0xff07080c));
        g.fillEllipse (pt.x - nodeR, pt.y - nodeR, nodeR * 2.0f, nodeR * 2.0f);

        // Glowing colored ring
        g.setColour (col);
        g.drawEllipse (pt.x - nodeR, pt.y - nodeR, nodeR * 2.0f, nodeR * 2.0f, 2.0f);

        // Solid pure white center core dot (matching reference 1:1)
        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (pt.x - 2.2f, pt.y - 2.2f, 4.4f, 4.4f);

        // Label Text
        g.setFont (juce::FontOptions ("Segoe UI", 9.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xffe2e8f0));

        if (label == "BRIGHT")
            g.drawText (label, (int) (pt.x - 40.0f), (int) (pt.y - nodeR - 15.0f), 80, 14, juce::Justification::centred);
        else
            g.drawText (label, (int) (pt.x - 40.0f), (int) (pt.y + nodeR + 3.0f), 80, 14, juce::Justification::centred);
    }

    // 8. Draw Central Draggable Control Puck — 1:1 Pure White Glowing Ring with Translucent Frosted Glass
    void drawControlPuck (juce::Graphics& g, juce::Point<float> pos)
    {
        const float puckR = 13.5f + (rmsOut * 2.5f);

        // Soft outer shadow & ambient halo
        g.setColour (juce::Colour (0x45000000));
        g.fillEllipse (pos.x - puckR - 3.5f, pos.y - puckR - 3.5f, (puckR + 3.5f) * 2.0f, (puckR + 3.5f) * 2.0f);

        g.setColour (juce::Colour (0x22ffffff));
        g.fillEllipse (pos.x - puckR - 2.0f, pos.y - puckR - 2.0f, (puckR + 2.0f) * 2.0f, (puckR + 2.0f) * 2.0f);

        // Translucent frosted glass fill
        g.setColour (juce::Colour (0x2cffffff));
        g.fillEllipse (pos.x - puckR, pos.y - puckR, puckR * 2.0f, puckR * 2.0f);

        // Bold pure white radiant ring (matching reference 1:1)
        g.setColour (juce::Colour (0xffffffff));
        g.drawEllipse (pos.x - puckR, pos.y - puckR, puckR * 2.0f, puckR * 2.0f, 2.6f);
    }

    // 9. Draw Bottom Controls (SPACE Slider & Real-Time Phase Correlation Meter)
    void drawBottomBar (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        float bottomY = bounds.getBottom() - 18.0f;

        // SPACE: XX% Label
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xff38bdf8));
        juce::String spaceText = "SPACE: " + juce::String ((int) spaceSize) + "%";
        g.drawText (spaceText, (int) bounds.getX() + 10, (int) bottomY, 66, 14, juce::Justification::centredLeft);

        // Rainbow Gradient Slider Bar
        auto barRect = getSpaceSliderBarBounds (bounds);
        g.setColour (juce::Colour (0xff0e1017));
        g.fillRoundedRectangle (barRect, 2.5f);

        // Soft rainbow gradient stroke along the bar
        juce::ColourGradient rainbowGrad (juce::Colour (0xff38bdf8), barRect.getX(), barRect.getY(),
                                          juce::Colour (0xff10b981), barRect.getRight(), barRect.getY(), false);
        rainbowGrad.addColour (0.33, juce::Colour (0xff818cf8));
        rainbowGrad.addColour (0.66, juce::Colour (0xfff472b6));
        g.setGradientFill (rainbowGrad);

        float fillNorm = (spaceSize - 10.0f) / 90.0f;
        auto fillRect = barRect.withWidth (barRect.getWidth() * fillNorm);
        g.fillRoundedRectangle (fillRect, 2.5f);

        // Draggable thumb indicator
        float thumbX = barRect.getX() + barRect.getWidth() * fillNorm;
        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (thumbX - 4.0f, barRect.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colour (0xff00f0ff));
        g.drawEllipse (thumbX - 4.0f, barRect.getCentreY() - 4.0f, 8.0f, 8.0f, 1.2f);

        // Real-Time Stereo Phase Correlation Meter & Readout
        const int meterW = 44;
        const int meterH = 4;
        const int meterX = (int) bounds.getRight() - meterW - 10;
        const int meterY = (int) bounds.getBottom() - 13;

        g.setColour (juce::Colour (0xff0c0d12));
        g.fillRoundedRectangle ((float) meterX, (float) meterY, (float) meterW, (float) meterH, 2.0f);
        g.setColour (juce::Colour (0xff1e222e));
        g.drawRoundedRectangle ((float) meterX, (float) meterY, (float) meterW, (float) meterH, 2.0f, 0.8f);

        // Center mono tick
        g.setColour (juce::Colour (0x66ffffff));
        g.drawVerticalLine (meterX + meterW / 2, (float) meterY, (float) (meterY + meterH));

        float pinNorm = juce::jlimit (0.0f, 1.0f, (phaseCorr + 1.0f) * 0.5f);
        float pinX = meterX + pinNorm * (float) (meterW - 4);
        juce::Colour phaseCol = (phaseCorr > 0.35f) ? juce::Colour (0xff10b981)
                              : (phaseCorr >= 0.0f) ? juce::Colour (0xfff59e0b)
                              : juce::Colour (0xffef4444);

        g.setColour (phaseCol);
        g.fillRoundedRectangle (pinX, (float) meterY - 1.0f, 3.5f, (float) (meterH + 2), 1.5f);

        juce::String phaseText = juce::String ("PHASE: ") + (phaseCorr >= 0.0f ? "+" : "") + juce::String (phaseCorr, 2) + " |";
        g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
        g.setColour (phaseCol.withAlpha (0.95f));
        g.drawText (phaseText, meterX - 78, (int) bounds.getBottom() - 18, 74, 14, juce::Justification::right);
    }

    // 10. Draw AI Acoustic Scan Overlay
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

        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xffffffff));
        g.drawText ("AI ACOUSTIC SCAN: " + juce::String ((int)(analyzeProgress * 100.0f)) + "%",
                    (int) centre.x - 90, (int) centre.y - 10, 180, 20, juce::Justification::centred);
    }
};
