#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <vector>
#include <functional>
#include <algorithm>
#include "ProVerbTokens.h"

/**
 * SpaceVisualizer — Interactive 3D Acoustic Space Prism
 * Producer Toy | ProVerb Smart Algorithmic Reverb
 *
 * Faithfully preserves the authentic ProVerb volumetric acoustic prism:
 * - Large, rounded, translucent 3D triangular glass body with rich volumetric glow
 * - Multi-gradient spectrum: Cyan apex, Magenta base-left, Mint base-right,
 *   Violet left slope, Golden-Amber bottom center, and Center Opal Core bloom
 * - 16 internal breathing resonance wavefront contours
 * - 3 layered concentric ghost echo hulls
 * - 5 radar orbital distance rings, glowing side arcs, and satellite nodes
 * - Continuously animated liquid water-wave borders running along all 3 sides
 *   and seamlessly travelling around all 3 rounded corners in standby and playback
 * - Live, Subtle, Freeze modes & 3D VIEW / 2D VIEW perspective switching
 */
class SpaceVisualizer : public juce::Component
{
public:
    enum class PrismMode
    {
        Live = 0,
        Subtle = 1,
        Freeze = 2
    };

    SpaceVisualizer()
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    std::function<void(float bright, float warm, float spacious)> onAcousticBalanceChanged;
    std::function<void(float newSizePercent)> onSpaceSizeChanged;

    void setPrismMode (PrismMode mode) noexcept
    {
        prismMode = mode;
        repaint();
    }

    PrismMode getPrismMode() const noexcept { return prismMode; }
    bool is3DView() const noexcept { return is3DMode; }
    void set3DView (bool is3D) noexcept
    {
        is3DMode = is3D;
        repaint();
    }

    std::function<void()> onAlgoModeClicked;
    std::function<void()> onToneColorClicked;

    void setAlgorithmModeName (const juce::String& name)
    {
        if (currentAlgoName != name)
        {
            currentAlgoName = name;
            repaint();
        }
    }

    void setTonalColorName (const juce::String& name)
    {
        if (currentToneName != name)
        {
            currentToneName = name;
            repaint();
        }
    }

    // Comprehensive audio telemetry & DSP parameter ingestion
    void updateAudioTelemetry (float newRmsIn, float newPeakIn, float newRmsOut, float newRmsWet,
                               float newBass, float newMid, float newTreble,
                               float newTransient, float newPhaseCorr, float newDuckingDb,
                               float sizePercent, float decaySec,
                               float diffusionPercent, float dampingPercent,
                               float predelayMs, float shimmerPercent,
                               float lowCutHz = 120.0f, float highCutHz = 12000.0f,
                               float duckPercent = 0.0f, float widthPercent = 100.0f)
    {
        rmsIn = newRmsIn;
        peakIn = newPeakIn;
        rmsOut = newRmsOut;
        rmsWet = newRmsWet;
        rawBass = newBass;
        rawMid = newMid;
        rawTreble = newTreble;
        rawTransient = newTransient;
        phaseCorr = newPhaseCorr;
        duckingGainReductionDb = newDuckingDb;

        spaceSize = juce::jlimit (10.0f, 100.0f, sizePercent);
        decayTime = juce::jlimit (0.1f, 30.0f, decaySec);
        diffusion = juce::jlimit (0.0f, 100.0f, diffusionPercent);
        damping = juce::jlimit (0.0f, 100.0f, dampingPercent);
        predelay = juce::jlimit (0.0f, 300.0f, predelayMs);
        shimmer = juce::jlimit (0.0f, 100.0f, shimmerPercent);
        lowCut = juce::jlimit (20.0f, 2000.0f, lowCutHz);
        highCut = juce::jlimit (500.0f, 20000.0f, highCutHz);
        ducking = juce::jlimit (0.0f, 100.0f, duckPercent);
        stereoWidth = juce::jlimit (0.0f, 200.0f, widthPercent);

        // Telemetry ballistic smoothing (unless frozen)
        if (prismMode != PrismMode::Freeze)
        {
            float sensitivity = (prismMode == PrismMode::Subtle) ? 0.35f : 1.0f;

            float targetBass = rawBass * sensitivity;
            float targetMid  = rawMid * sensitivity;
            float targetTreb = rawTreble * sensitivity;
            float targetWet  = rmsWet * sensitivity;

            smoothBass += (targetBass > smoothBass ? 0.35f : 0.08f) * (targetBass - smoothBass);
            smoothMid  += (targetMid  > smoothMid  ? 0.30f : 0.09f) * (targetMid  - smoothMid);
            smoothTreble += (targetTreb > smoothTreble ? 0.40f : 0.12f) * (targetTreb - smoothTreble);
            smoothWet  += (targetWet  > smoothWet  ? 0.28f : 0.05f) * (targetWet  - smoothWet);

            if (rawTransient > smoothTransient)
                smoothTransient = rawTransient * sensitivity;
            else
                smoothTransient *= 0.88f;

            // Audio-reactive strobe trigger on transient impact
            if (rawTransient > 0.30f && prismMode != PrismMode::Freeze)
            {
                float strobeAmt = rawTransient * (prismMode == PrismMode::Subtle ? 0.35f : 0.85f);
                if (strobeAmt > strobeIntensity)
                    strobeIntensity = juce::jlimit (0.0f, 1.0f, strobeAmt);
            }

            bool isSilent = (rmsIn < 0.0015f && rmsWet < 0.0015f);
            float targetSilence = isSilent ? 1.0f : 0.0f;
            silenceFactor += (targetSilence - silenceFactor) * 0.06f;

            float targetDuck = (duckingGainReductionDb < -0.5f)
                ? juce::jlimit (0.25f, 1.0f, std::pow (10.0f, duckingGainReductionDb / 20.0f))
                : 1.0f;
            duckingFactor += (targetDuck - duckingFactor) * 0.25f;
        }

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

    // Backward-compatible parameter updater
    void updateAllParameters (float newRmsIn, float newRmsOut,
                              float sizePercent, float decaySec,
                              float diffusionPercent, float dampingPercent,
                              float predelayMs, float shimmerPercent,
                              float newPhaseCorr = 1.0f,
                              float lowCutHz = 120.0f, float highCutHz = 12000.0f,
                              float duckPercent = 0.0f)
    {
        updateAudioTelemetry (newRmsIn, newRmsIn * 1.4f, newRmsOut, newRmsOut * 0.85f,
                              newRmsIn * 0.45f, newRmsIn * 0.35f, newRmsIn * 0.25f,
                              0.0f, newPhaseCorr, 0.0f,
                              sizePercent, decaySec, diffusionPercent, dampingPercent,
                              predelayMs, shimmerPercent, lowCutHz, highCutHz, duckPercent, 100.0f);
    }

    void setAnalyzing (bool analyzing, float progress = 0.0f)
    {
        isAnalyzing = analyzing;
        analyzeProgress = progress;
        repaint();
    }

    void advanceAnimation (float deltaPhase)
    {
        if (prismMode != PrismMode::Freeze)
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
        }

        // Liquid Water-Wave Border animation (Always running continuously in standby, idle, and music)
        waterPhase += deltaPhase * 2.2f;
        if (waterPhase > juce::MathConstants<float>::twoPi * 100.0f)
            waterPhase -= juce::MathConstants<float>::twoPi * 100.0f;

        // --- REAL ELASTIC SPRING-MASS PHYSICS SIMULATION ---
        const float dt = juce::jlimit (0.01f, 0.05f, deltaPhase);
        const float stiffness = 160.0f; // Spring restitution constant k
        const float dampingVal = 13.0f; // Velocity damping factor c

        // Squeeze & compression target forces
        float targetDisp[3] = { 0.0f, 0.0f, 0.0f };

        // 1. Interactive Mouse Press / Corner Squeeze
        if (isSqueezingVertex && pressedVertex >= 0 && pressedVertex < 3)
        {
            targetDisp[pressedVertex] = dragSqueezeTarget[pressedVertex];
            // Conservation of volume: squashing one vertex slightly pushes out the other two
            int o1 = (pressedVertex + 1) % 3;
            int o2 = (pressedVertex + 2) % 3;
            targetDisp[o1] -= dragSqueezeTarget[pressedVertex] * 0.28f;
            targetDisp[o2] -= dragSqueezeTarget[pressedVertex] * 0.28f;
        }

        // 2. Interactive Puck Proximity Squeeze (Puck mass pushing against corners)
        if (isDraggingPuck)
        {
            const float weights[3] = { weightBright, weightWarm, weightSpace };
            for (int i = 0; i < 3; ++i)
            {
                if (weights[i] > 0.45f)
                {
                    float push = (weights[i] - 0.45f) * 45.0f;
                    targetDisp[i] += push;
                    int o1 = (i + 1) % 3;
                    int o2 = (i + 2) % 3;
                    targetDisp[o1] -= push * 0.25f;
                    targetDisp[o2] -= push * 0.25f;
                }
            }
        }

        // 3. Audio Acoustic Pressure Pulsing (Bass compression & transient impact)
        float bassPulse = smoothBass * 14.0f;
        float transientPulse = smoothTransient * 18.0f;
        float audioCompress = (bassPulse + transientPulse) * (prismMode == PrismMode::Subtle ? 0.35f : (prismMode == PrismMode::Freeze ? 0.0f : 1.0f));

        targetDisp[0] += audioCompress * 0.5f;
        targetDisp[1] += audioCompress * 0.7f;
        targetDisp[2] += audioCompress * 0.7f;

        for (int i = 0; i < 3; ++i)
        {
            float springForce = -stiffness * (vertexDisp[i] - targetDisp[i]) - dampingVal * vertexVel[i];
            vertexVel[i] += springForce * dt;
            vertexDisp[i] += vertexVel[i] * dt;
            vertexDisp[i] = juce::jlimit (-16.0f, 38.0f, vertexDisp[i]);
        }

        // --- MUSIC REACTIVE MICRO-SHAKE & STROBE ---
        float shakeAmp = (smoothBass * 2.2f + smoothTransient * 3.8f);
        if (prismMode == PrismMode::Freeze) shakeAmp = 0.0f;
        else if (prismMode == PrismMode::Subtle) shakeAmp *= 0.35f;

        shakeX = (std::sin (waterPhase * 24.0f) * 0.7f + std::sin (waterPhase * 43.0f) * 0.3f) * shakeAmp;
        shakeY = (std::cos (waterPhase * 27.0f) * 0.7f + std::cos (waterPhase * 51.0f) * 0.3f) * shakeAmp;

        // Exponential decay for transient strobe flash
        strobeIntensity *= 0.86f;

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
        geom.centre = juce::Point<float> (bounds.getCentreX() + shakeX, bounds.getY() + bounds.getHeight() * 0.46f + shakeY);

        float minDim = juce::jmin (bounds.getWidth(), bounds.getHeight());
        geom.radius = minDim * 0.38f;

        auto baseV0 = getVertexPosition (geom.centre, geom.radius, geom.scaleY, 0);
        auto baseV1 = getVertexPosition (geom.centre, geom.radius, geom.scaleY, 1);
        auto baseV2 = getVertexPosition (geom.centre, geom.radius, geom.scaleY, 2);

        // Apply physical inward/outward squeeze displacement
        auto dir0 = (baseV0 - geom.centre);
        auto dir1 = (baseV1 - geom.centre);
        auto dir2 = (baseV2 - geom.centre);

        float len0 = std::hypot (dir0.x, dir0.y);
        float len1 = std::hypot (dir1.x, dir1.y);
        float len2 = std::hypot (dir2.x, dir2.y);

        geom.vBright   = baseV0 - (dir0 / (len0 > 0.001f ? len0 : 1.0f)) * vertexDisp[0];
        geom.vWarm     = baseV1 - (dir1 / (len1 > 0.001f ? len1 : 1.0f)) * vertexDisp[1];
        geom.vSpacious = baseV2 - (dir2 / (len2 > 0.001f ? len2 : 1.0f)) * vertexDisp[2];

        geom.puckPos = juce::Point<float> (
            weightBright * geom.vBright.x + weightWarm * geom.vWarm.x + weightSpace * geom.vSpacious.x,
            weightBright * geom.vBright.y + weightWarm * geom.vWarm.y + weightSpace * geom.vSpacious.y
        );

        return geom;
    }

    // Soft Harmonious Pastel Spectrum Colors
    juce::Colour getLiveColorBright() const
    {
        auto c1 = juce::Colour (0xff38d9f5); // Soft Ethereal Cyan
        auto c2 = juce::Colour (0xff4b8aff); // Soft Sky Blue
        return c1.interpolatedWith (c2, 0.25f);
    }

    juce::Colour getLiveColorWarm() const
    {
        auto c1 = juce::Colour (0xffea60b2); // Soft Blossom Rose
        auto c2 = juce::Colour (0xfff07090); // Soft Coral
        return c1.interpolatedWith (c2, 0.25f);
    }

    juce::Colour getLiveColorSpacious() const
    {
        auto c1 = juce::Colour (0xff42dfb0); // Soft Seafoam Mint
        auto c2 = juce::Colour (0xff38d9f5); // Soft Cyan
        return c1.interpolatedWith (c2, 0.25f);
    }

    juce::Colour getLiveColorAmber() const
    {
        auto c1 = juce::Colour (0xffffaf60); // Soft Peach Amber
        auto c2 = juce::Colour (0xffff9050); // Soft Gold
        return c1.interpolatedWith (c2, 0.20f);
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

        // 2. Panel Header: "| ACOUSTIC SPACE" (Pills removed per user request)
        drawHeader (g, bounds);

        auto geom = getGeometry (bounds);

        auto colBright   = getLiveColorBright();
        auto colWarm     = getLiveColorWarm();
        auto colSpacious = getLiveColorSpacious();
        auto colAmber    = getLiveColorAmber();

        // 3. Ambient Volumetric Atmospheric Glows & Glass Body Fill (Original ProVerb Prism Core with Soft Colors)
        drawAtmosphericNebula (g, geom.centre, geom.radius, geom.vBright, geom.vWarm, geom.vSpacious, geom.puckPos,
                               colBright, colWarm, colSpacious, colAmber);

        // 4. Concentric Layered Ghost Echo Hulls (Audio-Reactive Fluid Side Waves)
        drawLayeredGhostHulls (g, geom.centre, geom.vBright, geom.vWarm, geom.vSpacious,
                               colBright, colWarm, colSpacious);

        // (5. Radar Orbital Circles removed completely per user instruction: "ye sare cirlcels hata")

        // 6. Resonance Wavefront Contours (16 internal breathing waves)
        drawResonanceContours (g, geom.vBright, geom.vWarm, geom.vSpacious, geom.puckPos, geom.radius);

        // 7. Liquid Water-Wave Prism Borders (Continuous fluid ripples, traveling glints, & refractive meniscus)
        drawAcousticShell (g, geom.vBright, geom.vWarm, geom.vSpacious);

        // 8. Shimmer Floating Particles
        if (shimmer > 1.0f || smoothTreble > 0.08f)
        {
            drawShimmerParticles (g, geom.centre, geom.radius, geom.scaleY);
        }

        // 9. Vertex Anchor Badges (BRIGHT, WARM, SPACIOUS)
        drawVertexNode (g, geom.vBright,   "BRIGHT",   colBright,   weightBright);
        drawVertexNode (g, geom.vWarm,     "WARM",     colWarm,     weightWarm);
        drawVertexNode (g, geom.vSpacious, "SPACIOUS", colSpacious, weightSpace);

        // 10. Central Control Puck (Blends harmoniously with underlying colors, no harsh white glow)
        drawControlPuck (g, geom.puckPos, colBright, colWarm, colSpacious);

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
        g.setColour (juce::Colour (0xff38d9f5));
        g.fillRoundedRectangle (headX, headY + 2.0f, 2.5f, 11.0f, 1.0f);

        // Section Title
        g.setFont (ProVerbTokens::Typography::makeHeaderFont (10.0f, true));
        g.setColour (juce::Colour (0xfff2f6fc));
        g.drawText ("ACOUSTIC SPACE", (int) (headX + 8.0f), (int) headY, 120, 15, juce::Justification::centredLeft);

        // Tonal Color Pill
        auto toneRect = getToneColorBounds (bounds);
        g.setColour (juce::Colour (0xff101b29));
        g.fillRoundedRectangle (toneRect, 4.0f);
        g.setColour (juce::Colour (0xff243246));
        g.drawRoundedRectangle (toneRect, 4.0f, 1.0f);
        g.setFont (ProVerbTokens::Typography::makeLabelFont (8.5f, false));
        g.setColour (juce::Colour (0xff8b9ab0));
        g.drawText (currentToneName, toneRect.toNearestInt(), juce::Justification::centred, false);

        // Algorithm Mode Pill
        auto algoRect = getAlgoModeBounds (bounds);
        g.setColour (juce::Colour (0xff101b29));
        g.fillRoundedRectangle (algoRect, 4.0f);
        g.setColour (juce::Colour (0xff243246));
        g.drawRoundedRectangle (algoRect, 4.0f, 1.0f);
        g.setFont (ProVerbTokens::Typography::makeLabelFont (8.5f, true));
        g.setColour (juce::Colour (0xff38bdf8));
        g.drawText (currentAlgoName + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe")), algoRect.toNearestInt(), juce::Justification::centred, false);

        // 3D/2D View Toggle Pill
        auto viewRect = get2DViewButtonBounds (bounds);
        g.setColour (juce::Colour (0xff101b29));
        g.fillRoundedRectangle (viewRect, 4.0f);
        g.setColour (juce::Colour (0xff243246));
        g.drawRoundedRectangle (viewRect, 4.0f, 1.0f);
        g.setFont (ProVerbTokens::Typography::makeLabelFont (8.5f, false));
        g.setColour (is3DMode ? juce::Colour (0xff35e6b5) : juce::Colour (0xff8b9ab0));
        g.drawText (is3DMode ? "3D" : "2D", viewRect.toNearestInt(), juce::Justification::centred, false);
    }

    juce::Point<float> getVertexPosition (juce::Point<float> centre, float radius, float scaleY, int vertexIndex) const
    {
        float baseAngle = -juce::MathConstants<float>::halfPi;
        float angle = baseAngle + (float) vertexIndex * (juce::MathConstants<float>::twoPi / 3.0f);

        float vx = centre.x + std::cos (angle) * radius;
        float vy = centre.y + std::sin (angle) * radius * scaleY;
        return { vx, vy };
    }

    juce::Rectangle<float> getSpaceSliderBarBounds (juce::Rectangle<float> bounds) const
    {
        float barX = bounds.getX() + 92.0f;
        float barW = bounds.getWidth() - 250.0f;
        float barH = 5.0f;
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

    // Soft Volumetric Glass Body & Atmospheric Nebula
    void drawAtmosphericNebula (juce::Graphics& g, juce::Point<float> centre, float radius,
                                juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2,
                                juce::Point<float> puckPos,
                                juce::Colour colBright, juce::Colour colWarm,
                                juce::Colour colSpacious, juce::Colour colAmber)
    {
        juce::ignoreUnused (puckPos);
        const float audioPulse = (prismMode != PrismMode::Freeze) ? (rmsOut * 0.25f + smoothBass * 0.20f) : 0.0f;
        const float r = radius * (1.0f + audioPulse * 0.05f);

        auto midBottom = (v1 + v2) * 0.5f;
        auto midLeft   = (v0 + v1) * 0.5f;

        const float driftX = (prismMode != PrismMode::Freeze) ? (std::sin (colorMorphPhase) * (r * 0.02f)) : 0.0f;
        const float driftY = (prismMode != PrismMode::Freeze) ? (std::cos (colorMorphPhase * 0.85f) * (r * 0.02f)) : 0.0f;

        // Exterior soft diffused atmospheric glows
        float leftGlowX = centre.x - r * 0.85f + driftX;
        float leftGlowY = centre.y - r * 0.05f + driftY;
        juce::ColourGradient sideLeftGlow (colWarm.withAlpha (0.24f), leftGlowX, leftGlowY,
                                           colWarm.withAlpha (0.0f), leftGlowX - r * 0.90f, leftGlowY, true);
        g.setGradientFill (sideLeftGlow);
        g.fillEllipse (leftGlowX - r * 0.90f, leftGlowY - r * 0.90f, r * 1.80f, r * 1.80f);

        float rightGlowX = centre.x + r * 0.85f - driftX;
        float rightGlowY = centre.y - r * 0.05f + driftY;
        juce::ColourGradient sideRightGlow (colSpacious.withAlpha (0.24f), rightGlowX, rightGlowY,
                                            colSpacious.withAlpha (0.0f), rightGlowX + r * 0.90f, rightGlowY, true);
        g.setGradientFill (sideRightGlow);
        g.fillEllipse (rightGlowX - r * 0.90f, rightGlowY - r * 0.90f, r * 1.80f, r * 1.80f);

        float topGlowX = centre.x + driftX * 0.5f;
        float topGlowY = v0.y - r * 0.08f + driftY * 0.5f;
        juce::ColourGradient topGlow (colBright.withAlpha (0.26f), topGlowX, topGlowY,
                                      colBright.withAlpha (0.0f), topGlowX, topGlowY + r * 0.85f, true);
        g.setGradientFill (topGlow);
        g.fillEllipse (topGlowX - r * 0.85f, topGlowY - r * 0.60f, r * 1.70f, r * 1.40f);

        // Rounded Triangle Path
        juce::Path trianglePath;
        buildRoundedTrianglePath (trianglePath, v0, v1, v2, 24.0f);

        // Soft frosted translucent dark glass fill
        g.setColour (juce::Colour (0xc8070b12));
        g.fillPath (trianglePath);

        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (trianglePath);

        const float duckFactor = juce::jlimit (0.35f, 1.0f, (1.0f - (ducking / 100.0f) * (rmsIn * 2.5f)) * duckingFactor);
        const float warmGain = juce::jlimit (0.25f, 1.15f, 1.05f - ((lowCut - 20.0f) / 980.0f) * 0.45f);
        const float brightGain = juce::jlimit (0.30f, 1.25f, 0.4f + ((highCut - 1000.0f) / 19000.0f) * 0.85f);

        // Top Apex Soft Ethereal Sky Cyan
        float cyanAlpha = juce::jlimit (0.18f, 0.44f, (0.32f + weightBright * 0.12f + smoothTreble * 0.12f) * brightGain * duckFactor);
        juce::ColourGradient gradCyan (colBright.withAlpha (cyanAlpha), v0.x + driftX, v0.y + driftY,
                                       colBright.withAlpha (0.0f), v0.x, centre.y + r * 0.28f, true);
        g.setGradientFill (gradCyan);
        g.fillEllipse (v0.x - r * 0.90f, v0.y - r * 0.45f, r * 1.80f, r * 1.50f);

        // Bottom-Left Soft Blossom Rose / Magenta
        float warmAlpha = juce::jlimit (0.18f, 0.42f, (0.30f + weightWarm * 0.12f + smoothBass * 0.12f) * warmGain * duckFactor);
        juce::ColourGradient gradWarm (colWarm.withAlpha (warmAlpha), v1.x - driftX, v1.y + driftY,
                                       colWarm.withAlpha (0.0f), centre.x + r * 0.18f, centre.y - r * 0.18f, true);
        g.setGradientFill (gradWarm);
        g.fillEllipse (v1.x - r * 0.90f, v1.y - r * 0.80f, r * 1.80f, r * 1.60f);

        // Bottom-Right Soft Seafoam Mint
        float spaceAlpha = juce::jlimit (0.18f, 0.42f, (0.30f + weightSpace * 0.12f + smoothWet * 0.14f) * duckFactor);
        juce::ColourGradient gradSpace (colSpacious.withAlpha (spaceAlpha), v2.x + driftX, v2.y - driftY,
                                        colSpacious.withAlpha (0.0f), centre.x - r * 0.18f, centre.y - r * 0.18f, true);
        g.setGradientFill (gradSpace);
        g.fillEllipse (v2.x - r * 0.90f, v2.y - r * 0.80f, r * 1.80f, r * 1.60f);

        // Left Slope Soft Lavender Violet
        auto colViolet = juce::Colour (0xffa78bfa);
        float violetAlpha = (0.24f + (weightBright + weightWarm) * 0.10f) * duckFactor;
        juce::ColourGradient gradViolet (colViolet.withAlpha (violetAlpha), midLeft.x, midLeft.y,
                                         colViolet.withAlpha (0.0f), centre.x, centre.y, true);
        g.setGradientFill (gradViolet);
        g.fillEllipse (midLeft.x - r * 0.60f, midLeft.y - r * 0.60f, r * 1.20f, r * 1.20f);

        // Bottom Center Soft Peach Amber
        float amberAlpha = (0.26f + (weightWarm + weightSpace) * 0.08f) * duckFactor;
        juce::ColourGradient bottomAmber (colAmber.withAlpha (amberAlpha), midBottom.x, midBottom.y + 4.0f,
                                          colAmber.withAlpha (0.0f), midBottom.x, midBottom.y - r * 0.45f, true);
        g.setGradientFill (bottomAmber);
        g.fillEllipse (midBottom.x - r * 0.70f, midBottom.y - r * 0.35f, r * 1.40f, r * 0.80f);
    }

    // Audio-Reactive Fluid Ghost Hulls / Side Waves
    void drawLayeredGhostHulls (juce::Graphics& g, juce::Point<float> centre,
                                juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2,
                                juce::Colour colBright, juce::Colour colWarm, juce::Colour colSpacious)
    {
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float baseScales[3] = { 1.09f, 1.20f, 1.32f };
        const float waveSens = (prismMode == PrismMode::Subtle) ? 0.35f : (prismMode == PrismMode::Freeze ? 0.0f : 1.0f);

        for (int i = 0; i < 3; ++i)
        {
            // Expansion breathes with reverb wet energy, bass, and decay
            float scale = baseScales[i] + (smoothWet * (0.04f + (float) i * 0.02f) + smoothBass * 0.025f) * waveSens;

            auto ev0 = centre + (v0 - centre) * scale;
            auto ev1 = centre + (v1 - centre) * scale;
            auto ev2 = centre + (v2 - centre) * scale;

            std::vector<LiquidPerimeterSample> hullSamples;
            getRoundedTriangleSamples (hullSamples, ev0, ev1, ev2, 60);

            if (hullSamples.empty())
                continue;

            float amp1 = (1.4f + smoothBass * 2.2f + strobeIntensity * 1.8f) * waveSens;
            float amp2 = (0.8f + smoothTreble * 1.4f) * waveSens;
            float t = waterPhase + (float) i * 1.25f;

            juce::Path wavyHull;
            for (size_t sIdx = 0; sIdx < hullSamples.size(); ++sIdx)
            {
                float s = hullSamples[sIdx].s;
                float w = amp1 * std::sin (3.0f * twoPi * s - t * 1.6f)
                        + amp2 * std::sin (6.0f * twoPi * s + t * 1.2f);

                auto pt = hullSamples[sIdx].basePos + hullSamples[sIdx].normal * w;
                if (sIdx == 0)
                    wavyHull.startNewSubPath (pt);
                else
                    wavyHull.lineTo (pt);
            }
            wavyHull.closeSubPath();

            // Soft atmospheric chromatic gradient stroke
            float strokeAlpha = juce::jlimit (0.05f, 0.45f, (0.16f + smoothWet * 0.22f + strobeIntensity * 0.18f) * (1.0f - (float) i * 0.25f));
            juce::ColourGradient hullStroke (colBright.withAlpha (strokeAlpha), ev0.x, ev0.y,
                                             colWarm.withAlpha (strokeAlpha * 0.85f), ev1.x, ev1.y, false);
            hullStroke.addColour (0.58, colSpacious.withAlpha (strokeAlpha * 0.85f));
            g.setGradientFill (hullStroke);
            g.strokePath (wavyHull, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Delicate ambient glow fill
            float fillAlpha = juce::jlimit (0.005f, 0.05f, (0.018f + smoothWet * 0.025f) * (1.0f - (float) i * 0.3f));
            juce::ColourGradient hullFill (colBright.withAlpha (fillAlpha), centre.x, centre.y - 30.0f,
                                           colWarm.withAlpha (fillAlpha * 0.8f), centre.x - 30.0f, centre.y + 30.0f, false);
            hullFill.addColour (0.6, colSpacious.withAlpha (fillAlpha * 0.8f));
            g.setGradientFill (hullFill);
            g.fillPath (wavyHull);
        }
    }

    // Radar Orbital Circles removed completely per user instruction ("ye sare cirlcels hata")
    void drawOrbitalRings (juce::Graphics&, juce::Point<float>, float, float,
                           juce::Colour, juce::Colour, juce::Colour, juce::Colour)
    {
    }

    // Original 16 Resonance Wavefront Contours
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

    struct LiquidPerimeterSample
    {
        juce::Point<float> basePos;
        juce::Point<float> normal;
        float s { 0.0f };
    };

    void getRoundedTriangleSamples (std::vector<LiquidPerimeterSample>& outSamples,
                                    juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2,
                                    int totalSamples = 84) const
    {
        outSamples.clear();
        outSamples.reserve ((size_t) totalSamples);

        const auto c = (v0 + v1 + v2) / 3.0f;
        const juce::Point<float> pts[3] = { v0, v1, v2 };
        const float cut = 0.22f;

        juce::Point<float> startPt[3];
        juce::Point<float> endPt[3];
        juce::Point<float> ctrlStraight[3];

        for (int i = 0; i < 3; ++i)
        {
            int prev = (i + 2) % 3;
            int next = (i + 1) % 3;
            endPt[i]   = pts[i] + (pts[prev] - pts[i]) * cut;
            startPt[i] = pts[i] + (pts[next] - pts[i]) * cut;
        }

        for (int i = 0; i < 3; ++i)
        {
            int next = (i + 1) % 3;
            auto midStraight = (startPt[i] + endPt[next]) * 0.5f;
            auto outward = midStraight - c;
            ctrlStraight[i] = midStraight + outward * 0.22f;
        }

        const int samplesPerSide = totalSamples / 3;
        const int edgeSamples = (samplesPerSide * 65) / 100;
        const int cornerSamples = samplesPerSide - edgeSamples;

        auto evalQuad = [](juce::Point<float> p0, juce::Point<float> p1, juce::Point<float> p2, float u,
                           juce::Point<float>& pt, juce::Point<float>& tangent)
        {
            float inv = 1.0f - u;
            pt = p0 * (inv * inv) + p1 * (2.0f * inv * u) + p2 * (u * u);
            tangent = (p1 - p0) * (2.0f * inv) + (p2 - p1) * (2.0f * u);
        };

        for (int i = 0; i < 3; ++i)
        {
            int next = (i + 1) % 3;

            // 1. Edge Segment
            for (int k = 0; k < edgeSamples; ++k)
            {
                float u = (float) k / (float) edgeSamples;
                juce::Point<float> pos, tan;
                evalQuad (startPt[i], ctrlStraight[i], endPt[next], u, pos, tan);

                float len = std::hypot (tan.x, tan.y);
                juce::Point<float> n = (len > 0.001f) ? juce::Point<float> (tan.y / len, -tan.x / len) : juce::Point<float> (0.0f, 0.0f);
                if ((pos - c).x * n.x + (pos - c).y * n.y < 0.0f)
                {
                    n.x = -n.x;
                    n.y = -n.y;
                }

                float s = (float) outSamples.size() / (float) totalSamples;
                outSamples.push_back ({ pos, n, s });
            }

            // 2. Corner Segment
            for (int k = 0; k < cornerSamples; ++k)
            {
                float u = (float) k / (float) cornerSamples;
                juce::Point<float> pos, tan;
                evalQuad (endPt[next], pts[next], startPt[next], u, pos, tan);

                float len = std::hypot (tan.x, tan.y);
                juce::Point<float> n = (len > 0.001f) ? juce::Point<float> (tan.y / len, -tan.x / len) : juce::Point<float> (0.0f, 0.0f);
                if ((pos - c).x * n.x + (pos - c).y * n.y < 0.0f)
                {
                    n.x = -n.x;
                    n.y = -n.y;
                }

                float s = (float) outSamples.size() / (float) totalSamples;
                outSamples.push_back ({ pos, n, s });
            }
        }
    }

    // Liquid Water-Wave Prism Border Engine (Subtle fluid ripples applied to the existing prism perimeter)
    void drawAcousticShell (juce::Graphics& g, juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2)
    {
        // 1. Original baseline shell path
        juce::Path shellPath;
        buildRoundedTrianglePath (shellPath, v0, v1, v2, 24.0f);

        // Sample closed rounded triangular perimeter
        std::vector<LiquidPerimeterSample> samples;
        getRoundedTriangleSamples (samples, v0, v1, v2, 84);

        if (samples.empty())
        {
            // Fallback to static strokes if sampling fails
            g.setColour (juce::Colour (0x1524d7ff));
            g.strokePath (shellPath, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colour (0x3524d7ff));
            g.strokePath (shellPath, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colour (0x8bf2f6fc));
            g.strokePath (shellPath, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            return;
        }

        // Extremely subtle liquid displacement preserving the clean original silhouette
        float t = waterPhase;
        float sens = (prismMode == PrismMode::Subtle) ? 0.35f : 1.0f;
        if (prismMode == PrismMode::Freeze) sens = 0.0f;

        float a1 = 1.4f + (smoothBass * 2.2f + smoothTransient * 2.8f) * sens;
        float a2 = 0.8f + (smoothMid * 1.4f) * sens;
        float a3 = 0.4f + (smoothTreble * 1.0f) * sens;

        juce::Path primaryLiquidPath;
        juce::Path secondaryLiquidPath;
        juce::Path outerAuraPath;

        const float twoPi = juce::MathConstants<float>::twoPi;

        for (size_t idx = 0; idx < samples.size(); ++idx)
        {
            const auto& sp = samples[idx];
            float s = sp.s;

            // Multi-frequency periodic wave (k = 3, 6, 9 guarantee smooth continuity around all corners)
            float wave1 = std::sin (3.0f * twoPi * s - t * 2.0f);
            float wave2 = std::sin (6.0f * twoPi * s + t * 1.5f + 1.2f);
            float wave3 = std::sin (9.0f * twoPi * s - t * 3.0f + 2.5f);

            float totalDelta = a1 * wave1 + a2 * wave2 + a3 * wave3;
            float secDelta   = a2 * std::cos (6.0f * twoPi * s - t * 1.6f) + 2.6f;
            float auraDelta  = totalDelta + 5.5f;

            auto ptPrimary   = sp.basePos + sp.normal * totalDelta;
            auto ptSecondary = sp.basePos + sp.normal * (totalDelta + secDelta);
            auto ptAura      = sp.basePos + sp.normal * auraDelta;

            if (idx == 0)
            {
                primaryLiquidPath.startNewSubPath (ptPrimary);
                secondaryLiquidPath.startNewSubPath (ptSecondary);
                outerAuraPath.startNewSubPath (ptAura);
            }
            else
            {
                primaryLiquidPath.lineTo (ptPrimary);
                secondaryLiquidPath.lineTo (ptSecondary);
                outerAuraPath.lineTo (ptAura);
            }
        }

        primaryLiquidPath.closeSubPath();
        secondaryLiquidPath.closeSubPath();
        outerAuraPath.closeSubPath();

        // Layer 1: Outer Liquid Glow Aura (soft illuminated water meniscus)
        float wetFactor = juce::jlimit (0.35f, 1.0f, 0.45f + smoothWet * 1.5f + (decayTime / 30.0f) * 0.35f);
        wetFactor *= duckingFactor;

        juce::ColourGradient auraGrad (juce::Colour (0xff24d7ff).withAlpha (0.16f * wetFactor), v0.x, v0.y,
                                       juce::Colour (0xfff34db7).withAlpha (0.12f * wetFactor), v1.x, v1.y, false);
        auraGrad.addColour (0.6, juce::Colour (0xff35e6b5).withAlpha (0.14f * wetFactor));
        g.setGradientFill (auraGrad);
        g.strokePath (outerAuraPath, juce::PathStrokeType (6.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Layer 2: Secondary Exterior Liquid Wave Contour (refractive light ripple)
        juce::ColourGradient secGrad (juce::Colour (0xff35e6b5).withAlpha (0.35f * wetFactor), v2.x, v2.y,
                                      juce::Colour (0xff24d7ff).withAlpha (0.30f * wetFactor), v0.x, v0.y, false);
        secGrad.addColour (0.5, juce::Colour (0xff9460ff).withAlpha (0.28f * wetFactor));
        g.setGradientFill (secGrad);
        g.strokePath (secondaryLiquidPath, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Layer 3: Primary Illuminated Liquid Glass Border
        juce::ColourGradient primGrad (juce::Colour (0xff24d7ff).withAlpha (0.85f * wetFactor), v0.x, v0.y,
                                       juce::Colour (0xfff34db7).withAlpha (0.80f * wetFactor), v1.x, v1.y, false);
        primGrad.addColour (0.58, juce::Colour (0xff35e6b5).withAlpha (0.82f * wetFactor));
        g.setGradientFill (primGrad);
        g.strokePath (primaryLiquidPath, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Layer 4: Original White Surface Reflection Core Ribbon
        g.setColour (juce::Colour (0x8bf2f6fc).withAlpha (0.65f * wetFactor));
        g.strokePath (primaryLiquidPath, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Layer 5: Traveling Specular Water Glints (3 gliding light droplets along perimeter and around corners)
        const int numGlints = 3;
        for (int k = 0; k < numGlints; ++k)
        {
            float sGlint = std::fmod (t * 0.08f + (float) k / (float) numGlints, 1.0f);
            size_t idxA = (size_t) (sGlint * (float) samples.size()) % samples.size();
            size_t idxB = (idxA + 1) % samples.size();
            float fract = (sGlint * (float) samples.size()) - (float) idxA;

            auto ptA = samples[idxA].basePos + samples[idxA].normal * (a1 * std::sin (3.0f * twoPi * samples[idxA].s - t * 2.0f));
            auto ptB = samples[idxB].basePos + samples[idxB].normal * (a1 * std::sin (3.0f * twoPi * samples[idxB].s - t * 2.0f));
            auto glintPos = ptA + (ptB - ptA) * fract;

            float glintBrightness = 0.70f + smoothTreble * 0.30f + (shimmer / 100.0f) * 0.25f;
            g.setColour (juce::Colour (0xff24d7ff).withAlpha (0.40f * glintBrightness));
            g.fillEllipse (glintPos.x - 4.5f, glintPos.y - 4.5f, 9.0f, 9.0f);
            g.setColour (juce::Colour (0xffffffff).withAlpha (0.92f * glintBrightness));
            g.fillEllipse (glintPos.x - 2.0f, glintPos.y - 2.0f, 4.0f, 4.0f);
        }
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

        g.setFont (ProVerbTokens::Typography::makeLabelFont (9.5f, true));
        g.setColour (juce::Colour (0xfff2f6fc));

        if (label == "BRIGHT")
            g.drawText (label, (int) (pt.x - 45.0f), (int) (pt.y - nodeR - 16.0f), 90, 15, juce::Justification::centred);
        else
            g.drawText (label, (int) (pt.x - 45.0f), (int) (pt.y + nodeR + 3.0f), 90, 15, juce::Justification::centred);
    }

    // Optical Color-Blending Control Puck (Frosted glass lens that absorbs and blends with local colors)
    void drawControlPuck (juce::Graphics& g, juce::Point<float> pos,
                          juce::Colour colBright, juce::Colour colWarm, juce::Colour colSpacious)
    {
        const float puckR = 13.5f + (rmsOut * 1.5f);

        // Barycentrically blend the color under the puck so it absorbs the local atmosphere
        float totalW = weightBright + weightWarm + weightSpace;
        float wB = weightBright / (totalW > 0.001f ? totalW : 1.0f);
        float wW = weightWarm   / (totalW > 0.001f ? totalW : 1.0f);
        float wS = weightSpace  / (totalW > 0.001f ? totalW : 1.0f);

        juce::Colour localTint = colBright.interpolatedWith (colWarm, wW / (wB + wW + 0.0001f))
                                 .interpolatedWith (colSpacious, wS);

        // 1. Soft ambient shadow drop
        g.setColour (juce::Colour (0x35000000));
        g.fillEllipse (pos.x - puckR - 3.0f, pos.y - puckR - 1.5f, (puckR + 3.0f) * 2.0f, (puckR + 3.0f) * 2.0f);

        // 2. Translucent Frosted Glass Body that blends with underlying colors
        juce::ColourGradient lensGrad (localTint.withAlpha (0.42f), pos.x - puckR * 0.5f, pos.y - puckR * 0.5f,
                                       juce::Colour (0x12070b12), pos.x + puckR, pos.y + puckR, false);
        g.setGradientFill (lensGrad);
        g.fillEllipse (pos.x - puckR, pos.y - puckR, puckR * 2.0f, puckR * 2.0f);

        // 3. Subtle rim stroke: blends between soft white reflection on top and local color on bottom
        juce::ColourGradient rimGrad (juce::Colour (0x75ffffff), pos.x, pos.y - puckR,
                                      localTint.withAlpha (0.50f), pos.x, pos.y + puckR, false);
        g.setGradientFill (rimGrad);
        g.drawEllipse (pos.x - puckR, pos.y - puckR, puckR * 2.0f, puckR * 2.0f, 1.5f);

        // 4. Subtle inner lens highlight
        g.setColour (juce::Colour (0x24ffffff));
        g.fillEllipse (pos.x - puckR * 0.45f, pos.y - puckR * 0.55f, puckR * 0.70f, puckR * 0.40f);

        // 5. Delicate center pin dot
        g.setColour (juce::Colour (0xb0ffffff));
        g.fillEllipse (pos.x - 2.0f, pos.y - 2.0f, 4.0f, 4.0f);
    }

    void drawBottomBar (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        float bottomY = bounds.getBottom() - 22.0f;

        // SPACE: XX% Label
        g.setFont (ProVerbTokens::Typography::makeLabelFont (9.5f, true));
        g.setColour (juce::Colour (0xff38d9f5));
        juce::String spaceText = "SPACE: " + juce::String ((int) spaceSize) + "%";
        g.drawText ("SPACE: " + juce::String ((int) spaceSize) + "%", (int) bounds.getX() + 14, (int) bottomY, 74, 16, juce::Justification::centredLeft);

        // Rainbow Gradient Slider Bar
        auto barRect = getSpaceSliderBarBounds (bounds);
        g.setColour (juce::Colour (0xff0a111b));
        g.fillRoundedRectangle (barRect, 2.5f);

        juce::ColourGradient rainbowGrad (juce::Colour (0xff38d9f5), barRect.getX(), barRect.getY(),
                                          juce::Colour (0xff42dfb0), barRect.getRight(), barRect.getY(), false);
        rainbowGrad.addColour (0.28, juce::Colour (0xffa78bfa));
        rainbowGrad.addColour (0.55, juce::Colour (0xffea60b2));
        rainbowGrad.addColour (0.78, juce::Colour (0xffffaf60));
        g.setGradientFill (rainbowGrad);

        float fillNorm = (spaceSize - 10.0f) / 90.0f;
        auto fillRect = barRect.withWidth (barRect.getWidth() * fillNorm);
        g.fillRoundedRectangle (fillRect, 2.5f);

        // Draggable thumb indicator
        float thumbX = barRect.getX() + barRect.getWidth() * fillNorm;
        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (thumbX - 4.5f, barRect.getCentreY() - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colour (0xff38d9f5));
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
        juce::Colour phaseCol = (phaseCorr > 0.35f) ? juce::Colour (0xff42dfb0)
                              : (phaseCorr >= 0.0f) ? juce::Colour (0xffffaf60)
                              : juce::Colour (0xffff4d6d);

        g.setColour (phaseCol);
        g.fillRoundedRectangle (pinX, (float) meterY - 1.0f, 3.5f, (float) (meterH + 2), 1.5f);

        juce::String phaseText = juce::String ("PHASE: ") + (phaseCorr >= 0.0f ? "+" : "") + juce::String (phaseCorr, 2) + " |";
        g.setFont (ProVerbTokens::Typography::makeLabelFont (9.0f, true));
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

        g.setFont (ProVerbTokens::Typography::makeHeaderFont (10.5f, true));
        g.setColour (juce::Colour (0xffffffff));
        g.drawText ("AI ACOUSTIC SCAN: " + juce::String ((int)(analyzeProgress * 100.0f)) + "%",
                    (int) centre.x - 100, (int) centre.y - 10, 200, 20, juce::Justification::centred);
    }

    // Button Bounds (Kept for API compatibility)
    juce::Rectangle<float> getLiveModeButtonBounds (juce::Rectangle<float> bounds) const
    {
        return juce::Rectangle<float> (bounds.getRight() - 238.0f, bounds.getY() + 8.0f, 36.0f, 18.0f);
    }

    juce::Rectangle<float> getSubtleModeButtonBounds (juce::Rectangle<float> bounds) const
    {
        return juce::Rectangle<float> (bounds.getRight() - 198.0f, bounds.getY() + 8.0f, 44.0f, 18.0f);
    }

    juce::Rectangle<float> getFreezeModeButtonBounds (juce::Rectangle<float> bounds) const
    {
        return juce::Rectangle<float> (bounds.getRight() - 150.0f, bounds.getY() + 8.0f, 44.0f, 18.0f);
    }

    juce::Rectangle<float> get3DViewButtonBounds (juce::Rectangle<float> bounds) const
    {
        return juce::Rectangle<float> (bounds.getRight() - 100.0f, bounds.getY() + 8.0f, 48.0f, 18.0f);
    }

    juce::Rectangle<float> getAlgoModeBounds (juce::Rectangle<float> bounds) const
    {
        return juce::Rectangle<float> (bounds.getRight() - 176.0f, bounds.getY() + 8.0f, 126.0f, 18.0f);
    }

    juce::Rectangle<float> getToneColorBounds (juce::Rectangle<float> bounds) const
    {
        return juce::Rectangle<float> (bounds.getRight() - 246.0f, bounds.getY() + 8.0f, 64.0f, 18.0f);
    }

    juce::Rectangle<float> get2DViewButtonBounds (juce::Rectangle<float> bounds) const
    {
        return juce::Rectangle<float> (bounds.getRight() - 46.0f, bounds.getY() + 8.0f, 38.0f, 18.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 0. Check Header Pills (Algorithm Mode, Tonal Color, 3D/2D View)
        if (getAlgoModeBounds (bounds).contains (e.position))
        {
            if (onAlgoModeClicked)
                onAlgoModeClicked();
            return;
        }

        if (getToneColorBounds (bounds).contains (e.position))
        {
            if (onToneColorClicked)
                onToneColorClicked();
            return;
        }

        if (get2DViewButtonBounds (bounds).contains (e.position))
        {
            set3DView (! is3DMode);
            return;
        }

        // 1. Check Bottom Space Slider Bar Click / Drag
        auto sliderBarRect = getSpaceSliderBarBounds (bounds);
        if (sliderBarRect.expanded (6.0f, 8.0f).contains (e.position))
        {
            isDraggingSpaceSlider = true;
            updateSpaceSliderFromMouse (e.position.x, sliderBarRect);
            return;
        }

        // 2. Interactive Squeeze: Check if clicking/pressing near a vertex/corner to squash it inward
        auto geom = getGeometry (bounds);
        juce::Point<float> verts[3] = { geom.vBright, geom.vWarm, geom.vSpacious };
        int nearestVert = -1;
        float minDist = 50.0f;
        for (int i = 0; i < 3; ++i)
        {
            float d = verts[i].getDistanceFrom (e.position);
            if (d < minDist)
            {
                minDist = d;
                nearestVert = i;
            }
        }

        if (nearestVert >= 0)
        {
            pressedVertex = nearestVert;
            isSqueezingVertex = true;
            dragSqueezeTarget[nearestVert] = 20.0f; // immediate inward squeeze
            vertexVel[nearestVert] += 85.0f;
            repaint();
            return;
        }

        // 3. Check Central Puck Drag
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

        // Dynamic Squeeze Drag: Dragging a corner inward squashes it with elastic physics
        if (isSqueezingVertex && pressedVertex >= 0 && pressedVertex < 3)
        {
            auto geom = getGeometry (bounds);
            auto vBase = getVertexPosition (geom.centre, geom.radius, geom.scaleY, pressedVertex);
            auto toCentre = geom.centre - vBase;
            float len = std::hypot (toCentre.x, toCentre.y);
            if (len > 0.001f)
            {
                auto norm = toCentre / len;
                float inward = (e.position - vBase).x * norm.x + (e.position - vBase).y * norm.y;
                dragSqueezeTarget[pressedVertex] = juce::jlimit (-6.0f, 36.0f, inward);
            }
            repaint();
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
        isSqueezingVertex = false;
        pressedVertex = -1;
        dragSqueezeTarget[0] = dragSqueezeTarget[1] = dragSqueezeTarget[2] = 0.0f;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto sliderBarRect = getSpaceSliderBarBounds (bounds).expanded (6.0f, 8.0f);

        if (sliderBarRect.contains (e.position))
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

private:
    juce::String currentAlgoName { "Neutral Room" };
    juce::String currentToneName { "Clean" };

    // Audio telemetry measurements
    float rmsIn { 0.0f };
    float peakIn { 0.0f };
    float rmsOut { 0.0f };
    float rmsWet { 0.0f };
    float rawBass { 0.0f };
    float rawMid { 0.0f };
    float rawTreble { 0.0f };
    float rawTransient { 0.0f };
    float phaseCorr { 1.0f };
    float duckingGainReductionDb { 0.0f };

    // Smoothed ballistic followers
    float smoothBass { 0.0f };
    float smoothMid { 0.0f };
    float smoothTreble { 0.0f };
    float smoothTransient { 0.0f };
    float smoothWet { 0.0f };
    float silenceFactor { 0.0f };
    float duckingFactor { 1.0f };

    // Real Elastic Spring-Mass Physics State (3 vertices: Bright = 0, Warm = 1, Spacious = 2)
    float vertexDisp[3] { 0.0f, 0.0f, 0.0f };
    float vertexVel[3]  { 0.0f, 0.0f, 0.0f };
    float dragSqueezeTarget[3] { 0.0f, 0.0f, 0.0f };
    int pressedVertex { -1 };
    bool isSqueezingVertex { false };

    // Music reactive micro-shake & strobe flash
    float shakeX { 0.0f };
    float shakeY { 0.0f };
    float strobeIntensity { 0.0f };

    // DSP Parameters
    float spaceSize { 70.0f };
    float decayTime { 2.8f };
    float diffusion { 75.0f };
    float damping { 45.0f };
    float predelay { 20.0f };
    float shimmer { 0.0f };
    float lowCut { 120.0f };
    float highCut { 12000.0f };
    float ducking { 0.0f };
    float stereoWidth { 100.0f };

    // Visual Modes
    PrismMode prismMode { PrismMode::Live };
    bool is3DMode { true };

    // Animation phases
    float rotationAngle { 0.0f };
    float shimmerPhase { 0.0f };
    float contourPulsePhase { 0.0f };
    float colorMorphPhase { 0.0f };
    float waterPhase { 0.0f };

    // Interaction states
    bool isDraggingPuck { false };
    bool isDraggingSpaceSlider { false };
    float weightBright { 0.333f };
    float weightWarm   { 0.333f };
    float weightSpace  { 0.333f };

    // AI Analysis scan overlay state
    bool isAnalyzing { false };
    float analyzeProgress { 0.0f };
};
