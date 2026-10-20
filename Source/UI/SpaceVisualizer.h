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
    enum class ViewMode
    {
        ThreeD,
        TwoD
    };

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
                              float newPhaseCorr = 1.0f)
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
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Futuristic Deep Obsidian Background
        g.setColour (juce::Colour (0xff06070a));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (juce::Colour (0xff181a24));
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

        // Active Acoustic Field Area (expanded cleanly without header clutter)
        auto fieldArea = bounds.reduced (10.0f, 10.0f);
        fieldArea.removeFromBottom (18.0f); // Space for bottom slider & phase indicator

        auto centre = fieldArea.getCentre();
        float radius = juce::jmin (fieldArea.getWidth(), fieldArea.getHeight()) * 0.48f;
        radius *= (0.82f + (spaceSize / 100.0f) * 0.18f); // Parameter Size expansion

        // 3D Perspective scale
        float scaleY = (currentViewMode == ViewMode::ThreeD) ? 0.78f : 1.0f;
        float yCenterOffset = (currentViewMode == ViewMode::ThreeD) ? (radius * 0.08f) : 0.0f;
        centre.y += yCenterOffset;

        // Calculate Triangle Vertices (Top = BRIGHT, Bottom-Left = WARM, Bottom-Right = SPACIOUS)
        auto vBright   = getVertexPosition (centre, radius, scaleY, 0);
        auto vWarm     = getVertexPosition (centre, radius, scaleY, 1);
        auto vSpacious = getVertexPosition (centre, radius, scaleY, 2);

        // Calculate Current Puck Position from weights
        juce::Point<float> puckPos (
            weightBright * vBright.x + weightWarm * vWarm.x + weightSpace * vSpacious.x,
            weightBright * vBright.y + weightWarm * vWarm.y + weightSpace * vSpacious.y
        );

        // 2. Draw Ambient Acoustic Glow & Soft Rainbow Multi-Gradient Nebula
        drawAtmosphericNebula (g, centre, radius, scaleY, vBright, vWarm, vSpacious, puckPos);

        // 3. Draw Orbital Rings and Satellite Acoustic Markers
        drawOrbitalRings (g, centre, radius, scaleY);

        // 4. Draw Concentric Acoustic Resonance Contours (Pulsing to audio & parameters)
        drawResonanceContours (g, puckPos, centre, radius, scaleY, vBright, vWarm, vSpacious);

        // 5. Draw Smooth Rounded-Triangular Acoustic Shell with Soft Blurred Chromatic Glow
        drawAcousticShell (g, vBright, vWarm, vSpacious, centre, radius, scaleY);

        // 6. Draw Shimmer Floating Particles
        if (shimmer > 1.0f)
        {
            drawShimmerParticles (g, centre, radius, scaleY);
        }

        // 7. Draw Vertex Anchor Badges (BRIGHT, WARM, SPACIOUS)
        drawVertexNode (g, vBright,   "BRIGHT",   juce::Colour (0xff38bdf8), weightBright);
        drawVertexNode (g, vWarm,     "WARM",     juce::Colour (0xfffb7185), weightWarm);
        drawVertexNode (g, vSpacious, "SPACIOUS", juce::Colour (0xff34d399), weightSpace);

        // 8. Draw Central Draggable Control Puck
        drawControlPuck (g, puckPos);

        // 9. Draw View Mode Toggle (3D VIEW | 2D VIEW) in top-right
        drawViewModeToggle (g, bounds);

        // 10. Draw Bottom Controls (SPACE Slider & Real-Time Phase Correlation Meter)
        drawBottomBar (g, bounds);

        // 11. Draw AI Acoustic Scan Overlay (if analyzing)
        if (isAnalyzing)
        {
            drawAnalysisOverlay (g, centre, radius);
        }
    }

    juce::Rectangle<float> getViewToggleBounds (juce::Rectangle<float> bounds) const
    {
        float toggleW = 104.0f;
        float toggleH = 20.0f;
        return juce::Rectangle<float> (bounds.getRight() - toggleW - 10.0f, bounds.getY() + 8.0f, toggleW, toggleH);
    }

    void drawViewModeToggle (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        auto toggleRect = getViewToggleBounds (bounds);
        float pillR = toggleRect.getHeight() * 0.48f;

        // Dark recessed capsule well
        g.setColour (juce::Colour (0xff0a0c12));
        g.fillRoundedRectangle (toggleRect, pillR);
        g.setColour (juce::Colour (0xff1e2330));
        g.drawRoundedRectangle (toggleRect, pillR, 1.0f);

        float halfW = toggleRect.getWidth() * 0.5f;
        auto leftSeg  = toggleRect.withWidth (halfW);
        auto rightSeg = toggleRect.withTrimmedLeft (halfW);

        bool is3D = (currentViewMode == ViewMode::ThreeD);
        auto activeSeg = is3D ? leftSeg.reduced (1.5f, 1.5f) : rightSeg.reduced (1.5f, 1.5f);

        // Soft active capsule pill
        juce::ColourGradient segGrad (juce::Colour (0xff0c2738), 0.0f, activeSeg.getY(),
                                      juce::Colour (0xff081a26), 0.0f, activeSeg.getBottom(), false);
        g.setGradientFill (segGrad);
        g.fillRoundedRectangle (activeSeg, activeSeg.getHeight() * 0.48f);

        g.setColour (juce::Colour (0xff00f0ff).withAlpha (0.75f));
        g.drawRoundedRectangle (activeSeg, activeSeg.getHeight() * 0.48f, 1.0f);

        g.setFont (juce::FontOptions (8.0f, juce::Font::bold));

        g.setColour (is3D ? juce::Colour (0xff00f0ff) : juce::Colour (0xff64748b));
        g.drawText ("3D VIEW", leftSeg, juce::Justification::centred, true);

        g.setColour (! is3D ? juce::Colour (0xff00f0ff) : juce::Colour (0xff64748b));
        g.drawText ("2D VIEW", rightSeg, juce::Justification::centred, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Check 3D / 2D View Toggle Click
        auto viewToggleRect = getViewToggleBounds (bounds);
        if (viewToggleRect.contains (e.position))
        {
            currentViewMode = (currentViewMode == ViewMode::ThreeD) ? ViewMode::TwoD : ViewMode::ThreeD;
            repaint();
            return;
        }

        // 2. Check Bottom Space Slider Bar Click / Drag
        auto sliderBarRect = getSpaceSliderBarBounds (bounds);
        if (sliderBarRect.expanded (4.0f, 6.0f).contains (e.position))
        {
            isDraggingSpaceSlider = true;
            updateSpaceSliderFromMouse (e.position.x, sliderBarRect);
            return;
        }

        // 3. Check Central Puck Drag
        auto fieldArea = bounds.reduced (10.0f, 10.0f);
        fieldArea.removeFromBottom (18.0f);
        auto centre = fieldArea.getCentre();
        float radius = juce::jmin (fieldArea.getWidth(), fieldArea.getHeight()) * 0.48f;
        radius *= (0.82f + (spaceSize / 100.0f) * 0.18f);
        float scaleY = (currentViewMode == ViewMode::ThreeD) ? 0.78f : 1.0f;
        float yCenterOffset = (currentViewMode == ViewMode::ThreeD) ? (radius * 0.08f) : 0.0f;
        centre.y += yCenterOffset;

        auto vBright   = getVertexPosition (centre, radius, scaleY, 0);
        auto vWarm     = getVertexPosition (centre, radius, scaleY, 1);
        auto vSpacious = getVertexPosition (centre, radius, scaleY, 2);

        juce::Point<float> puckPos (
            weightBright * vBright.x + weightWarm * vWarm.x + weightSpace * vSpacious.x,
            weightBright * vBright.y + weightWarm * vWarm.y + weightSpace * vSpacious.y
        );

        float distToPuck = puckPos.getDistanceFrom (e.position);
        float distToCenter = centre.getDistanceFrom (e.position);

        if (distToPuck <= 32.0f || distToCenter <= radius * 1.1f)
        {
            isDraggingPuck = true;
            updatePuckFromMouse (e.position, centre, radius, scaleY, vBright, vWarm, vSpacious);
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
            auto fieldArea = bounds.reduced (10.0f, 26.0f);
            fieldArea.removeFromBottom (16.0f);
            auto centre = fieldArea.getCentre();
            float radius = juce::jmin (fieldArea.getWidth(), fieldArea.getHeight()) * 0.46f;
            radius *= (0.82f + (spaceSize / 100.0f) * 0.18f);
            float scaleY = (currentViewMode == ViewMode::ThreeD) ? 0.78f : 1.0f;
            float yCenterOffset = (currentViewMode == ViewMode::ThreeD) ? (radius * 0.08f) : 0.0f;
            centre.y += yCenterOffset;

            auto vBright   = getVertexPosition (centre, radius, scaleY, 0);
            auto vWarm     = getVertexPosition (centre, radius, scaleY, 1);
            auto vSpacious = getVertexPosition (centre, radius, scaleY, 2);

            updatePuckFromMouse (e.position, centre, radius, scaleY, vBright, vWarm, vSpacious);
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
    ViewMode currentViewMode { ViewMode::ThreeD };

    float rmsIn = 0.0f;
    float rmsOut = 0.0f;
    float spaceSize = 70.0f;
    float decayTime = 2.8f;
    float diffusion = 75.0f;
    float damping = 45.0f;
    float predelay = 20.0f;
    float shimmer = 0.0f;
    float phaseCorr = 1.0f;

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
        float angle = 0.0f;
        if (index == 0) // Top
            angle = -juce::MathConstants<float>::halfPi;
        else if (index == 1) // Bottom-Left (approx 210 deg)
            angle = juce::MathConstants<float>::halfPi + juce::MathConstants<float>::pi / 3.0f;
        else // Bottom-Right (approx 330 deg)
            angle = juce::MathConstants<float>::halfPi - juce::MathConstants<float>::pi / 3.0f;

        float r = radius * 0.90f;
        return juce::Point<float> (
            centre.x + std::cos (angle) * r,
            centre.y + std::sin (angle) * r * scaleY
        );
    }

    juce::Rectangle<float> getSpaceSliderBarBounds (juce::Rectangle<float> bounds) const
    {
        float barW = 100.0f;
        float barH = 5.0f;
        float barX = bounds.getX() + 78.0f;
        float barY = bounds.getBottom() - 14.0f;
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

    void updatePuckFromMouse (juce::Point<float> mousePos, juce::Point<float> centre, float radius, float scaleY,
                             juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2)
    {
        juce::ignoreUnused (centre, radius, scaleY);

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

    // 2. Multi-gradient Atmospheric Aura Nebula (Soft Blurred Chromatic Rainbow Prism)
    void drawAtmosphericNebula (juce::Graphics& g, juce::Point<float> centre, float radius, float scaleY,
                                juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2,
                                juce::Point<float> puckPos)
    {
        juce::ignoreUnused (centre, scaleY);
        const float audioPulse = rmsOut * 0.38f;
        const float r = radius * (1.0f + audioPulse * 0.10f);

        // 1. Soft Rounded Triangle Shell Fill (Translucent Obsidian Glass Base)
        juce::Path baseShell;
        buildRoundedTrianglePath (baseShell, v0, v1, v2, 20.0f);
        g.setColour (juce::Colour (0xbb08090d));
        g.fillPath (baseShell);

        // Clip subsequent multi-spectral chromatic nebula inside the prism shell
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (baseShell);

        // 1. Red / Coral (#f43f5e) - Lower-Left anchor & warmth
        float redAlpha = (0.28f + weightWarm * 0.35f + audioPulse * 0.5f) * 0.70f;
        juce::ColourGradient gradRed (juce::Colour (0xfff43f5e).withAlpha (redAlpha), v1.x, v1.y,
                                      juce::Colour (0x00000000), puckPos.x, puckPos.y, true);
        g.setGradientFill (gradRed);
        g.fillEllipse (v1.x - r * 0.75f, v1.y - r * 0.65f, r * 1.50f, r * 1.30f);

        // 2. Orange / Amber (#f97316) - Warm lower-mid body
        auto midBottom = (v1 + v2) * 0.5f;
        float orangeAlpha = (0.26f + weightWarm * 0.30f + audioPulse * 0.4f) * 0.65f;
        juce::ColourGradient gradOrange (juce::Colour (0xfff97316).withAlpha (orangeAlpha), midBottom.x - r * 0.15f, midBottom.y,
                                         juce::Colour (0x00000000), midBottom.x, midBottom.y - r * 0.5f, true);
        g.setGradientFill (gradOrange);
        g.fillEllipse (midBottom.x - r * 0.60f, midBottom.y - r * 0.50f, r * 1.20f, r * 1.00f);

        // 3. Yellow / Gold (#eab308) - Center-bottom transition into spacious bloom
        float yellowAlpha = (0.26f + (weightWarm + weightSpace) * 0.18f + audioPulse * 0.4f) * 0.65f;
        juce::ColourGradient gradYellow (juce::Colour (0xffeab308).withAlpha (yellowAlpha), midBottom.x + r * 0.12f, midBottom.y,
                                         juce::Colour (0x00000000), midBottom.x + r * 0.12f, midBottom.y - r * 0.5f, true);
        g.setGradientFill (gradYellow);
        g.fillEllipse (midBottom.x - r * 0.45f, midBottom.y - r * 0.45f, r * 1.05f, r * 0.95f);

        // 4. Green / Emerald (#10b981) - Lower-Right diffusion & spaciousness
        float greenAlpha = (0.28f + weightSpace * 0.35f + audioPulse * 0.5f) * 0.70f;
        juce::ColourGradient gradGreen (juce::Colour (0xff10b981).withAlpha (greenAlpha), v2.x, v2.y,
                                        juce::Colour (0x00000000), puckPos.x, puckPos.y, true);
        g.setGradientFill (gradGreen);
        g.fillEllipse (v2.x - r * 0.75f, v2.y - r * 0.65f, r * 1.50f, r * 1.30f);

        // 5. Cyan / Aqua (#06b6d4) - Top vertex air & presence
        float cyanAlpha = (0.30f + weightBright * 0.38f + audioPulse * 0.5f) * 0.75f;
        juce::ColourGradient gradCyan (juce::Colour (0xff06b6d4).withAlpha (cyanAlpha), v0.x, v0.y,
                                       juce::Colour (0x00000000), puckPos.x, puckPos.y + r * 0.35f, true);
        g.setGradientFill (gradCyan);
        g.fillEllipse (v0.x - r * 0.75f, v0.y - r * 0.55f, r * 1.50f, r * 1.25f);

        // 6. Blue / Royal Indigo (#3b82f6) - Left slope atmospheric depth
        auto midLeft = (v0 + v1) * 0.5f;
        float blueAlpha = (0.24f + (weightBright + weightWarm) * 0.20f + audioPulse * 0.4f) * 0.65f;
        juce::ColourGradient gradBlue (juce::Colour (0xff3b82f6).withAlpha (blueAlpha), midLeft.x, midLeft.y,
                                       juce::Colour (0x00000000), puckPos.x, puckPos.y, true);
        g.setGradientFill (gradBlue);
        g.fillEllipse (midLeft.x - r * 0.60f, midLeft.y - r * 0.55f, r * 1.20f, r * 1.10f);

        // 7. Violet / Orchid Magenta (#a855f7) - Right slope shimmer & dimension
        auto midRight = (v0 + v2) * 0.5f;
        float violetAlpha = (0.24f + (weightBright + weightSpace) * 0.20f + audioPulse * 0.4f) * 0.65f;
        juce::ColourGradient gradViolet (juce::Colour (0xffa855f7).withAlpha (violetAlpha), midRight.x, midRight.y,
                                         juce::Colour (0x00000000), puckPos.x, puckPos.y, true);
        g.setGradientFill (gradViolet);
        g.fillEllipse (midRight.x - r * 0.60f, midRight.y - r * 0.55f, r * 1.20f, r * 1.10f);

        // Center Ambient Core Bloom (Soft Luminous Opal Glow around puck)
        float coreAlpha = (0.28f + (spaceSize / 100.0f) * 0.24f + audioPulse * 0.6f) * 0.55f;
        juce::ColourGradient coreBloom (juce::Colour (0xffffffff).withAlpha (coreAlpha), puckPos.x, puckPos.y,
                                        juce::Colour (0x00000000), puckPos.x, puckPos.y + r * 0.50f, true);
        g.setGradientFill (coreBloom);
        g.fillEllipse (puckPos.x - r * 0.55f, puckPos.y - r * 0.55f, r * 1.10f, r * 1.10f);
    }

    // 3. Orbital Rings with Acoustic Satellites (Neoverb-inspired futuristic depth)
    void drawOrbitalRings (juce::Graphics& g, juce::Point<float> centre, float radius, float scaleY)
    {
        float ringRadX = radius * 1.08f;
        float ringRadY = ringRadX * scaleY * 0.42f;

        // Subtle dark orbital ellipse
        g.setColour (juce::Colour (0x1e38bdf8));
        g.drawEllipse (centre.x - ringRadX, centre.y - ringRadY, ringRadX * 2.0f, ringRadY * 2.0f, 1.0f);

        // Orbiting satellite dots with soft blurred halos
        const int numDots = 3;
        for (int i = 0; i < numDots; ++i)
        {
            float dotAngle = rotationAngle + (float) i * (juce::MathConstants<float>::twoPi / (float) numDots);
            float dotX = centre.x + std::cos (dotAngle) * ringRadX;
            float dotY = centre.y + std::sin (dotAngle) * ringRadY;

            float dotAlpha = (std::sin (dotAngle) > 0.0f ? 0.85f : 0.30f);
            juce::Colour dotCol = (i == 0 ? juce::Colour (0xff00f0ff) : (i == 1 ? juce::Colour (0xffff5e7e) : juce::Colour (0xff10b981)));

            g.setColour (dotCol.withAlpha (dotAlpha * 0.30f));
            g.fillEllipse (dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);

            g.setColour (dotCol.withAlpha (dotAlpha));
            g.fillEllipse (dotX - 2.5f, dotY - 2.5f, 5.0f, 5.0f);

            g.setColour (juce::Colour (0xffffffff).withAlpha (dotAlpha));
            g.fillEllipse (dotX - 1.0f, dotY - 1.0f, 2.0f, 2.0f);
        }
    }

    static juce::Point<float> lerpPoint (juce::Point<float> a, juce::Point<float> b, float t)
    {
        return juce::Point<float> (a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
    }

    // 4. Concentric Acoustic Resonance Contours (Fine, Smooth, Atmospheric Waves)
    void drawResonanceContours (juce::Graphics& g, juce::Point<float> puckPos, juce::Point<float> centre,
                                float radius, float scaleY,
                                juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2)
    {
        juce::ignoreUnused (centre, radius, scaleY);
        const int numContours = 18;

        for (int c = 1; c <= numContours; ++c)
        {
            float norm = (float) c / (float) numContours;
            float waveOffset = std::sin (contourPulsePhase + norm * juce::MathConstants<float>::twoPi) * 0.035f;
            float contourWeight = juce::jlimit (0.05f, 0.98f, norm + waveOffset);

            // Interpolate vertices from puck position towards outer shell
            auto p0 = lerpPoint (puckPos, v0, contourWeight);
            auto p1 = lerpPoint (puckPos, v1, contourWeight);
            auto p2 = lerpPoint (puckPos, v2, contourWeight);

            juce::Path ringPath;
            buildRoundedTrianglePath (ringPath, p0, p1, p2, 8.0f * contourWeight);

            // Soft color gradient along contour
            float alpha = (1.0f - norm * 0.55f) * (0.10f + rmsOut * 0.28f);
            juce::Colour contourCol = juce::Colour (0xff00f0ff).interpolatedWith (juce::Colour (0xffff5e7e), norm * 0.5f);
            
            g.setColour (contourCol.withAlpha (alpha));
            g.strokePath (ringPath, juce::PathStrokeType (1.0f));
        }
    }

    // 5. Draw Smooth Rounded-Triangular Acoustic Shell (Multi-pass Atmospheric Blur Aura)
    void drawAcousticShell (juce::Graphics& g, juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2,
                            juce::Point<float> centre, float radius, float scaleY)
    {
        juce::ignoreUnused (centre, radius, scaleY);
        juce::Path shellPath;
        buildRoundedTrianglePath (shellPath, v0, v1, v2, 20.0f);

        // Multi-stage soft blurred chromatic aura (non-harsh, diffused atmospheric glow)
        // Pass 1: Wide outer cyan-violet bloom
        g.setColour (juce::Colour (0x1200f0ff));
        g.strokePath (shellPath, juce::PathStrokeType (14.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pass 2: Mid soft blue-magenta bloom
        g.setColour (juce::Colour (0x2438bdf8));
        g.strokePath (shellPath, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pass 3: Soft violet inner glow
        g.setColour (juce::Colour (0x44a855f7));
        g.strokePath (shellPath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pass 4: Soft silky core rim
        g.setColour (juce::Colour (0xa0e2e8f0));
        g.strokePath (shellPath, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Helper: construct a rounded triangle with curved convex edges
    void buildRoundedTrianglePath (juce::Path& p, juce::Point<float> p0, juce::Point<float> p1, juce::Point<float> p2, float cornerRadius)
    {
        juce::Point<float> pts[3] = { p0, p1, p2 };
        auto mid0 = (pts[0] + pts[1]) * 0.5f;
        auto mid1 = (pts[1] + pts[2]) * 0.5f;
        auto mid2 = (pts[2] + pts[0]) * 0.5f;

        // Mild outward bowing control points
        auto c = (p0 + p1 + p2) / 3.0f;
        mid0 += (mid0 - c) * 0.12f;
        mid1 += (mid1 - c) * 0.12f;
        mid2 += (mid2 - c) * 0.12f;

        float blend = juce::jlimit (0.01f, 0.45f, cornerRadius / 40.0f);
        auto startPt = lerpPoint (pts[0], mid0, blend);
        p.startNewSubPath (startPt);

        auto end1 = lerpPoint (pts[1], mid0, blend);
        p.quadraticTo (mid0.x, mid0.y, end1.x, end1.y);

        auto end2 = lerpPoint (pts[1], mid1, blend);
        p.quadraticTo (pts[1].x, pts[1].y, end2.x, end2.y);

        auto end3 = lerpPoint (pts[2], mid1, blend);
        p.quadraticTo (mid1.x, mid1.y, end3.x, end3.y);

        auto end4 = lerpPoint (pts[2], mid2, blend);
        p.quadraticTo (pts[2].x, pts[2].y, end4.x, end4.y);

        auto end5 = lerpPoint (pts[0], mid2, blend);
        p.quadraticTo (mid2.x, mid2.y, end5.x, end5.y);

        p.quadraticTo (pts[0].x, pts[0].y, startPt.x, startPt.y);
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

    // 7. Draw Vertex Anchor Badges (BRIGHT, WARM, SPACIOUS)
    void drawVertexNode (juce::Graphics& g, juce::Point<float> pt, const juce::String& label, juce::Colour col, float weight)
    {
        float nodeRadius = 6.0f + weight * 4.0f;

        // Outer soft bloom
        g.setColour (col.withAlpha (0.35f + weight * 0.40f));
        g.fillEllipse (pt.x - nodeRadius * 1.8f, pt.y - nodeRadius * 1.8f, nodeRadius * 3.6f, nodeRadius * 3.6f);

        // Core socket
        g.setColour (juce::Colour (0xff0a0c12));
        g.fillEllipse (pt.x - nodeRadius, pt.y - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);

        // Glowing ring
        g.setColour (col);
        g.drawEllipse (pt.x - nodeRadius, pt.y - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f, 1.8f);

        // Inner glowing jewel
        g.setColour (juce::Colour (0xffffffff).withAlpha (0.8f + weight * 0.2f));
        g.fillEllipse (pt.x - 2.0f, pt.y - 2.0f, 4.0f, 4.0f);

        // Label Text
        g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
        g.setColour (col.withAlpha (0.95f));

        if (label == "BRIGHT")
            g.drawText (label, (int) (pt.x - 40.0f), (int) (pt.y - nodeRadius - 16.0f), 80, 14, juce::Justification::centred);
        else if (label == "WARM")
            g.drawText (label, (int) (pt.x - 40.0f), (int) (pt.y + nodeRadius + 3.0f), 80, 14, juce::Justification::centred);
        else // SPACIOUS
            g.drawText (label, (int) (pt.x - 40.0f), (int) (pt.y + nodeRadius + 3.0f), 80, 14, juce::Justification::centred);
    }

    // 8. Draw Central Draggable Control Puck
    void drawControlPuck (juce::Graphics& g, juce::Point<float> pos)
    {
        float puckR = 11.0f + (rmsOut * 6.0f);

        // Outer diffused white/cyan aura
        g.setColour (juce::Colour (0x3300f0ff));
        g.fillEllipse (pos.x - puckR * 2.0f, pos.y - puckR * 2.0f, puckR * 4.0f, puckR * 4.0f);

        // Dark glass body
        g.setColour (juce::Colour (0xe608090d));
        g.fillEllipse (pos.x - puckR, pos.y - puckR, puckR * 2.0f, puckR * 2.0f);

        // Radiant pure white rim ring
        g.setColour (juce::Colour (0xffffffff));
        g.drawEllipse (pos.x - puckR, pos.y - puckR, puckR * 2.0f, puckR * 2.0f, 2.0f);

        // Center jewel dot
        float dotR = 3.5f + (rmsIn * 3.0f);
        g.setColour (juce::Colour (0xff38bdf8));
        g.fillEllipse (pos.x - dotR, pos.y - dotR, dotR * 2.0f, dotR * 2.0f);
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
