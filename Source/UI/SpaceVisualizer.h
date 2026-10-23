#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <functional>
#include "ProVerbTokens.h"

/**
 * SpaceVisualizer — Live 3D Audio-Reactive Prism Engine
 * Producer Toy | ProVerb Smart Algorithmic Reverb
 *
 * Fully interactive, real-time audio-reactive 3D crystal prism:
 * - 3D Perspective & 2D Planar projection modes
 * - Live, Subtle, and Freeze visual modes
 * - Driven by real audio telemetry:
 *     - Bass energy (<180 Hz) modulates prism base expansion
 *     - Mid energy (180 Hz - 3.2 kHz) modulates prism extrusion & facet contour
 *     - Treble energy (>3.2 kHz) modulates apex highlights & specular crystal glints
 *     - Transient & onset activity triggers radial shockwave ripple pulses
 *     - Reverb wet energy & decay time modulate ambient aura & trailing echo rings
 *     - Silence detection smoothly settles geometry into an elegant breathing idle
 * - Tightly bound to actual DSP parameters: Size, Diffusion, Decay, Pre-Delay,
 *   Damping, High Cut, Low Cut, Stereo Width, Shimmer, Ducking, Mix, and Output
 * - Multi-layer translucent geometry with true 3D rotation, depth-sorted faces,
 *   inner refractive core, glowing edge highlights, and spatial orbital rings
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

        // Low Cut attenuation on bass reaction
        float lowCutFactor = 1.0f;
        if (lowCut > 80.0f)
            lowCutFactor = juce::jlimit (0.05f, 1.0f, 1.0f - (lowCut - 80.0f) / 600.0f);

        // High Cut attenuation on treble reaction
        float highCutFactor = 1.0f;
        if (highCut < 14000.0f)
            highCutFactor = juce::jlimit (0.1f, 1.0f, (highCut - 1000.0f) / 13000.0f);

        // Telemetry ballistic smoothing (unless frozen)
        if (prismMode != PrismMode::Freeze)
        {
            float modeSensitivity = (prismMode == PrismMode::Subtle) ? 0.35f : 1.0f;

            // Attack / release ballistics
            float targetBass = rawBass * lowCutFactor * modeSensitivity;
            float targetMid  = rawMid * modeSensitivity;
            float targetTreb = rawTreble * highCutFactor * modeSensitivity;
            float targetWet  = rmsWet * modeSensitivity;

            smoothBass += (targetBass > smoothBass ? 0.35f : 0.08f) * (targetBass - smoothBass);
            smoothMid  += (targetMid  > smoothMid  ? 0.30f : 0.09f) * (targetMid  - smoothMid);
            smoothTreble += (targetTreb > smoothTreble ? 0.40f : 0.12f) * (targetTreb - smoothTreble);
            smoothWet  += (targetWet  > smoothWet  ? 0.28f : 0.05f) * (targetWet  - smoothWet);

            // Transient impulse follower
            if (rawTransient > smoothTransient)
            {
                smoothTransient = rawTransient * modeSensitivity;
                transientRingRadius = 0.1f;
                transientRingAlpha = 1.0f;
            }
            else
            {
                smoothTransient *= 0.88f;
            }

            // Silence detector: settles gracefully into calm breathing
            bool isSilent = (rmsIn < 0.0015f && rmsWet < 0.0015f);
            float targetSilence = isSilent ? 1.0f : 0.0f;
            silenceFactor += (targetSilence - silenceFactor) * 0.06f;

            // Ducking visual attenuation
            float targetDuckAtten = (duckingGainReductionDb < -0.5f)
                ? juce::jlimit (0.2f, 1.0f, std::pow (10.0f, duckingGainReductionDb / 20.0f))
                : 1.0f;
            duckingFactor += (targetDuckAtten - duckingFactor) * 0.25f;
        }

        // Update barycentric acoustic weights if puck not directly dragged
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

    // Backward-compatible overload for existing callers
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

    // Advance 60 FPS animation phases
    void advanceAnimation (float deltaPhase)
    {
        if (prismMode != PrismMode::Freeze)
        {
            // Dynamic rotation speed: modulated by stereo width & predelay, calm when silent
            float rotSpeed = (0.28f + (stereoWidth / 200.0f) * 0.20f + (predelay / 300.0f) * 0.15f);
            if (silenceFactor > 0.5f)
                rotSpeed *= (1.0f - silenceFactor * 0.65f); // Calm idle rotation

            yawAngle += deltaPhase * rotSpeed;
            if (yawAngle > juce::MathConstants<float>::twoPi)
                yawAngle -= juce::MathConstants<float>::twoPi;

            // Idle breathing oscillation
            idleBreathPhase += deltaPhase * 1.2f;
            if (idleBreathPhase > juce::MathConstants<float>::twoPi)
                idleBreathPhase -= juce::MathConstants<float>::twoPi;

            // Shimmer and orbital particle animation
            shimmerPhase += deltaPhase * (1.6f + (shimmer / 100.0f) * 2.0f);
            if (shimmerPhase > juce::MathConstants<float>::twoPi)
                shimmerPhase -= juce::MathConstants<float>::twoPi;

            // Expanding transient ring
            if (transientRingAlpha > 0.01f)
            {
                transientRingRadius += deltaPhase * 1.8f;
                transientRingAlpha *= 0.91f;
            }

            colorMorphPhase += deltaPhase * 0.4f;
            if (colorMorphPhase > juce::MathConstants<float>::twoPi * 10.0f)
                colorMorphPhase -= juce::MathConstants<float>::twoPi * 10.0f;
        }

        // Continuous Liquid Water-Wave Border animation (Always active, even in standby or silence)
        waterPhase += deltaPhase * 2.2f;
        if (waterPhase > juce::MathConstants<float>::twoPi * 100.0f)
            waterPhase -= juce::MathConstants<float>::twoPi * 100.0f;

        repaint();
    }

    // 3D Point Structure
    struct Point3D
    {
        float x { 0.0f };
        float y { 0.0f };
        float z { 0.0f };
    };

    // Projected 2D Point with Screen Depth
    struct ProjectedVertex
    {
        juce::Point<float> screen;
        float depthZ { 0.0f };
    };

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
        geom.centre = juce::Point<float> (bounds.getCentreX(), bounds.getY() + bounds.getHeight() * 0.47f);

        float minDim = juce::jmin (bounds.getWidth(), bounds.getHeight());
        // Space size parameter directly controls base radius [0.28 to 0.44]
        float sizeNorm = (spaceSize - 10.0f) / 90.0f;
        geom.radius = minDim * (0.28f + sizeNorm * 0.14f);

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
        // High Cut and Damping influence color temperature
        float temp = juce::jlimit (0.0f, 1.0f, (highCut - 4000.0f) / 16000.0f * (1.0f - damping / 160.0f));
        auto c1 = ProVerbTokens::Colors::neonCyan;
        auto c2 = ProVerbTokens::Colors::neonBlue;
        auto base = c1.interpolatedWith (c2, (1.0f - temp) * 0.5f);
        if (damping > 65.0f)
            base = base.interpolatedWith (ProVerbTokens::Colors::neonAmber, (damping - 65.0f) / 70.0f);
        return base;
    }

    juce::Colour getLiveColorWarm() const
    {
        auto c1 = ProVerbTokens::Colors::neonMagenta;
        auto c2 = ProVerbTokens::Colors::neonAmber;
        float wMod = juce::jlimit (0.0f, 1.0f, damping / 100.0f);
        return c1.interpolatedWith (c2, wMod * 0.35f);
    }

    juce::Colour getLiveColorSpacious() const
    {
        auto c1 = ProVerbTokens::Colors::neonMint;
        auto c2 = ProVerbTokens::Colors::neonCyan;
        float sMod = juce::jlimit (0.0f, 1.0f, spaceSize / 100.0f);
        return c1.interpolatedWith (c2, sMod * 0.40f);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Panel Background
        juce::ColourGradient bgGrad (ProVerbTokens::Colors::panelBackground, bounds.getX(), bounds.getY(),
                                     ProVerbTokens::Colors::backgroundSecondary, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (bgGrad);
        g.fillRoundedRectangle (bounds, ProVerbTokens::Metrics::panelCornerRadius);

        // Border #243246
        g.setColour (ProVerbTokens::Colors::border);
        g.drawRoundedRectangle (bounds, ProVerbTokens::Metrics::panelCornerRadius, 1.0f);

        // 2. Panel Header: "| ACOUSTIC SPACE", Mode Toggles, and 3D/2D View Toggles
        drawHeader (g, bounds);

        auto geom = getGeometry (bounds);
        auto colBright   = getLiveColorBright();
        auto colWarm     = getLiveColorWarm();
        auto colSpacious = getLiveColorSpacious();
        auto colAmber    = ProVerbTokens::Colors::neonAmber;

        // 3. Ambient Atmospheric Nebula Aura (reacts to wet reverb & ducking)
        drawAtmosphericNebula (g, geom.centre, geom.radius, colBright, colWarm, colSpacious, colAmber);

        // 4. Concentric Orbital Rings & Spatial Grid
        drawOrbitalRings (g, geom.centre, geom.radius, geom.scaleY, colBright, colWarm, colSpacious, colAmber);

        // 5. Transient Shockwave Impulse Ripple
        if (transientRingAlpha > 0.02f)
        {
            drawTransientRipple (g, geom.centre, geom.radius);
        }

        // 6. Centerpiece: 3D Live Prism or 2D Barycentric Acoustic Field
        if (is3DMode)
        {
            draw3DPrism (g, geom.centre, geom.radius, colBright, colWarm, colSpacious);
        }
        else
        {
            draw2DPlanarField (g, geom, colBright, colWarm, colSpacious);
        }

        // 7. Shimmer & Treble Floating Particles
        if (shimmer > 0.5f || smoothTreble > 0.05f)
        {
            drawShimmerParticles (g, geom.centre, geom.radius, geom.scaleY);
        }

        // 8. Vertex Badges: BRIGHT, WARM, SPACIOUS
        drawVertexNode (g, geom.vBright,   "BRIGHT",   colBright,   weightBright);
        drawVertexNode (g, geom.vWarm,     "WARM",     colWarm,     weightWarm);
        drawVertexNode (g, geom.vSpacious, "SPACIOUS", colSpacious, weightSpace);

        // 9. Central Interactive Frosted Glass Control Puck
        drawControlPuck (g, geom.puckPos);

        // 10. Bottom Bar: SPACE Slider & Real-Time Phase Correlation Meter
        drawBottomBar (g, bounds);

        // 11. AI Acoustic Scan Overlay (if active)
        if (isAnalyzing)
        {
            drawAnalysisOverlay (g, geom.centre, geom.radius);
        }
    }

    void drawHeader (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        float headY = bounds.getY() + 10.0f;
        float headX = bounds.getX() + 14.0f;

        // Section Accent Pip
        g.setColour (ProVerbTokens::Colors::neonCyan);
        g.fillRoundedRectangle (headX, headY + 2.0f, 2.5f, 11.0f, 1.0f);

        // Section Title
        g.setFont (ProVerbTokens::Typography::makeHeaderFont (11.0f, true));
        g.setColour (ProVerbTokens::Colors::textPrimary);
        g.drawText ("ACOUSTIC SPACE", (int) headX + 9, (int) headY, 130, 16, juce::Justification::centredLeft);

        // Draw [ LIVE | SUBTLE | FREEZE ] Mode Segmented Selector
        drawModePills (g, bounds);

        // Draw [ 3D VIEW | 2D VIEW ] Perspective Toggle
        drawViewPills (g, bounds);
    }

    void drawModePills (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        auto liveRect   = getLiveModeButtonBounds (bounds);
        auto subtleRect = getSubtleModeButtonBounds (bounds);
        auto freezeRect = getFreezeModeButtonBounds (bounds);

        auto drawPill = [&](juce::Rectangle<float> rect, const juce::String& text, bool active, juce::Colour activeGlow)
        {
            if (active)
            {
                g.setColour (activeGlow.withAlpha (0.16f));
                g.fillRoundedRectangle (rect.expanded (1.0f, 1.0f), 3.0f);
                g.setColour (ProVerbTokens::Colors::panelDarkSlot);
                g.fillRoundedRectangle (rect, 3.0f);
                g.setColour (activeGlow);
                g.drawRoundedRectangle (rect, 3.0f, 1.2f);
                g.setFont (ProVerbTokens::Typography::makeLabelFont (8.5f, true));
                g.setColour (activeGlow);
            }
            else
            {
                g.setColour (ProVerbTokens::Colors::panelBackground);
                g.fillRoundedRectangle (rect, 3.0f);
                g.setColour (ProVerbTokens::Colors::border);
                g.drawRoundedRectangle (rect, 3.0f, 0.8f);
                g.setFont (ProVerbTokens::Typography::makeLabelFont (8.5f, false));
                g.setColour (ProVerbTokens::Colors::textMuted);
            }
            g.drawText (text, rect.toNearestInt(), juce::Justification::centred);
        };

        drawPill (liveRect,   "LIVE",   prismMode == PrismMode::Live,   ProVerbTokens::Colors::neonCyan);
        drawPill (subtleRect, "SUBTLE", prismMode == PrismMode::Subtle, ProVerbTokens::Colors::neonMint);
        drawPill (freezeRect, "FREEZE", prismMode == PrismMode::Freeze, ProVerbTokens::Colors::neonAmber);
    }

    void drawViewPills (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        auto r3D = get3DViewButtonBounds (bounds);
        auto r2D = get2DViewButtonBounds (bounds);

        auto drawPill = [&](juce::Rectangle<float> rect, const juce::String& text, bool active)
        {
            if (active)
            {
                g.setColour (ProVerbTokens::Colors::neonBlue.withAlpha (0.18f));
                g.fillRoundedRectangle (rect.expanded (1.0f, 1.0f), 3.0f);
                g.setColour (ProVerbTokens::Colors::panelDarkSlot);
                g.fillRoundedRectangle (rect, 3.0f);
                g.setColour (ProVerbTokens::Colors::neonCyan);
                g.drawRoundedRectangle (rect, 3.0f, 1.2f);
                g.setFont (ProVerbTokens::Typography::makeLabelFont (9.0f, true));
                g.setColour (ProVerbTokens::Colors::neonCyan);
            }
            else
            {
                g.setColour (ProVerbTokens::Colors::panelBackground);
                g.fillRoundedRectangle (rect, 3.0f);
                g.setColour (ProVerbTokens::Colors::border);
                g.drawRoundedRectangle (rect, 3.0f, 0.8f);
                g.setFont (ProVerbTokens::Typography::makeLabelFont (9.0f, false));
                g.setColour (ProVerbTokens::Colors::textMuted);
            }
            g.drawText (text, rect.toNearestInt(), juce::Justification::centred);
        };

        drawPill (r3D, "3D VIEW", is3DMode);
        drawPill (r2D, "2D VIEW", ! is3DMode);
    }

    // 3D Perspective Projection Engine
    ProjectedVertex projectPoint (Point3D pt, juce::Point<float> screenCentre, float fovDistance = 380.0f) const
    {
        // 1. Rotation around Yaw axis
        float cosY = std::cos (yawAngle);
        float sinY = std::sin (yawAngle);
        float x1 = pt.x * cosY - pt.z * sinY;
        float z1 = pt.x * sinY + pt.z * cosY;
        float y1 = pt.y;

        // 2. Pitch tilt forward (~22 degrees for 3D depth)
        float pitch = is3DMode ? 0.38f : 0.0f;
        float cosP = std::cos (pitch);
        float sinP = std::sin (pitch);
        float y2 = y1 * cosP - z1 * sinP;
        float z2 = y1 * sinP + z1 * cosP;

        // 3. Perspective divide
        float zDist = fovDistance + z2;
        if (zDist < 20.0f) zDist = 20.0f;
        float proj = fovDistance / zDist;

        ProjectedVertex pv;
        pv.screen = juce::Point<float> (screenCentre.x + x1 * proj, screenCentre.y - y2 * proj);
        pv.depthZ = z2;
        return pv;
    }

    void draw3DPrism (juce::Graphics& g, juce::Point<float> centre, float baseRadius,
                      juce::Colour colB, juce::Colour colW, juce::Colour colS)
    {
        // Parameter-driven dimensions:
        // Size scales radius, Width scales X aspect, Diffusion softens facets
        float widthScale = (stereoWidth / 100.0f);
        float prismH = baseRadius * (0.85f + (spaceSize / 100.0f) * 0.45f);

        // Audio reactive modulations:
        // - Bass expands bottom base outward & down
        // - Mid expands prism height & facet bulge
        // - Treble sharpens apex vertices & specular glow
        float bassOffset = smoothBass * 42.0f;
        float midOffset  = smoothMid  * 30.0f;
        float trebOffset = smoothTreble * 22.0f;
        float transientPulse = smoothTransient * 26.0f;

        // Idle breathing when silent
        float idleBreathing = (silenceFactor > 0.05f) ? (std::sin (idleBreathPhase) * 4.5f * silenceFactor) : 0.0f;

        float topRadius    = (baseRadius + trebOffset + transientPulse + idleBreathing);
        float bottomRadius = (baseRadius * 1.10f + bassOffset + transientPulse + idleBreathing);
        float halfH = (prismH + midOffset + transientPulse) * 0.5f;

        // Construct 3D Vertices:
        // Triangle 0: Top face (Bright, Warm, Spacious) at Y = +halfH
        // Triangle 1: Bottom face (Bright, Warm, Spacious) at Y = -halfH
        Point3D top3D[3];
        Point3D bot3D[3];

        for (int i = 0; i < 3; ++i)
        {
            float ang = (float) i * juce::MathConstants<float>::twoPi / 3.0f - juce::MathConstants<float>::halfPi;
            top3D[i] = { std::cos (ang) * topRadius * widthScale,
                         halfH,
                         std::sin (ang) * topRadius };

            bot3D[i] = { std::cos (ang) * bottomRadius * widthScale,
                         -halfH,
                         std::sin (ang) * bottomRadius };
        }

        // Project 3D vertices to 2D screen
        ProjectedVertex topProj[3];
        ProjectedVertex botProj[3];
        for (int i = 0; i < 3; ++i)
        {
            topProj[i] = projectPoint (top3D[i], centre);
            botProj[i] = projectPoint (bot3D[i], centre);
        }

        // Inner Refractive Core (0.52x concentric mini-prism)
        ProjectedVertex coreTop[3];
        ProjectedVertex coreBot[3];
        for (int i = 0; i < 3; ++i)
        {
            coreTop[i] = projectPoint ({ top3D[i].x * 0.52f, top3D[i].y * 0.52f, top3D[i].z * 0.52f }, centre);
            coreBot[i] = projectPoint ({ bot3D[i].x * 0.52f, bot3D[i].y * 0.52f, bot3D[i].z * 0.52f }, centre);
        }

        // Facet structure for depth sorting
        struct Facet
        {
            int index { 0 };
            float avgZ { 0.0f };
            bool isTop { false };
            bool isBottom { false };
            int sideIndex { 0 };
        };

        std::vector<Facet> facets;
        facets.reserve (5);

        // Side Facet 0: Top0 -> Top1 -> Bot1 -> Bot0
        // Side Facet 1: Top1 -> Top2 -> Bot2 -> Bot1
        // Side Facet 2: Top2 -> Top0 -> Bot0 -> Bot2
        for (int i = 0; i < 3; ++i)
        {
            int next = (i + 1) % 3;
            float zAvg = (topProj[i].depthZ + topProj[next].depthZ + botProj[i].depthZ + botProj[next].depthZ) * 0.25f;
            facets.push_back ({ i, zAvg, false, false, i });
        }

        // Top Face
        float topZ = (topProj[0].depthZ + topProj[1].depthZ + topProj[2].depthZ) / 3.0f;
        facets.push_back ({ 3, topZ, true, false, 0 });

        // Bottom Face
        float botZ = (botProj[0].depthZ + botProj[1].depthZ + botProj[2].depthZ) / 3.0f;
        facets.push_back ({ 4, botZ, false, true, 0 });

        // Sort back to front (lowest depthZ drawn first)
        std::sort (facets.begin(), facets.end(), [](const Facet& a, const Facet& b) {
            return a.avgZ < b.avgZ;
        });

        // Wet tail aura & diffusion opacity
        float wetGlow = juce::jlimit (0.25f, 1.0f, 0.40f + (smoothWet * 2.2f) + (decayTime / 30.0f) * 0.35f);
        wetGlow *= duckingFactor; // Dims with ducking

        // Render sorted facets (Painter's Algorithm)
        for (const auto& f : facets)
        {
            if (f.isTop)
            {
                // Top Triangular Face (Bright, reflective crystalline surface)
                juce::Path topPath;
                topPath.startNewSubPath (topProj[0].screen);
                topPath.lineTo (topProj[1].screen);
                topPath.lineTo (topProj[2].screen);
                topPath.closeSubPath();

                juce::ColourGradient topGrad (colB.withAlpha (0.45f * wetGlow), topProj[0].screen.x, topProj[0].screen.y,
                                              colW.withAlpha (0.35f * wetGlow), topProj[1].screen.x, topProj[1].screen.y, false);
                topGrad.addColour (0.6, colS.withAlpha (0.38f * wetGlow));
                g.setGradientFill (topGrad);
                g.fillPath (topPath);

                // Edge glow
                g.setColour (colB.withAlpha (0.75f * wetGlow));
                g.strokePath (topPath, juce::PathStrokeType (1.8f));
            }
            else if (f.isBottom)
            {
                // Bottom Triangular Face
                juce::Path botPath;
                botPath.startNewSubPath (botProj[0].screen);
                botPath.lineTo (botProj[1].screen);
                botPath.lineTo (botProj[2].screen);
                botPath.closeSubPath();

                juce::Colour botCol = colW.interpolatedWith (ProVerbTokens::Colors::neonAmber, 0.4f);
                g.setColour (botCol.withAlpha (0.22f * wetGlow));
                g.fillPath (botPath);
                g.setColour (botCol.withAlpha (0.45f * wetGlow));
                g.strokePath (botPath, juce::PathStrokeType (1.2f));
            }
            else
            {
                // Side Quad Facet
                int i = f.sideIndex;
                int next = (i + 1) % 3;

                juce::Path sidePath;
                sidePath.startNewSubPath (topProj[i].screen);
                sidePath.lineTo (topProj[next].screen);
                sidePath.lineTo (botProj[next].screen);
                sidePath.lineTo (botProj[i].screen);
                sidePath.closeSubPath();

                juce::Colour cA = (i == 0 ? colB : (i == 1 ? colW : colS));
                juce::Colour cB = (next == 0 ? colB : (next == 1 ? colW : colS));

                juce::ColourGradient sideGrad (cA.withAlpha (0.38f * wetGlow), topProj[i].screen.x, topProj[i].screen.y,
                                               cB.withAlpha (0.24f * wetGlow), botProj[next].screen.x, botProj[next].screen.y, false);
                g.setGradientFill (sideGrad);
                g.fillPath (sidePath);

                // Specular Luminous Edge Highlight
                g.setColour (cA.withAlpha (0.70f * wetGlow));
                g.strokePath (sidePath, juce::PathStrokeType (1.4f));
            }
        }

        // Inner Floating Refractive Core
        juce::Path corePath;
        corePath.startNewSubPath (coreTop[0].screen);
        corePath.lineTo (coreTop[1].screen);
        corePath.lineTo (coreTop[2].screen);
        corePath.closeSubPath();
        g.setColour (ProVerbTokens::Colors::neonCyan.withAlpha (0.28f * wetGlow));
        g.fillPath (corePath);
        g.setColour (juce::Colour (0xffffffff).withAlpha (0.50f * wetGlow));
        g.strokePath (corePath, juce::PathStrokeType (1.0f));

        // Vertex Beacons on Top Corners
        for (int i = 0; i < 3; ++i)
        {
            auto pt = topProj[i].screen;
            juce::Colour vCol = (i == 0 ? colB : (i == 1 ? colW : colS));
            g.setColour (vCol.withAlpha (0.85f));
            g.fillEllipse (pt.x - 3.5f, pt.y - 3.5f, 7.0f, 7.0f);
            g.setColour (juce::Colour (0xffffffff));
            g.fillEllipse (pt.x - 1.5f, pt.y - 1.5f, 3.0f, 3.0f);
        }

        // 7. Liquid Water-Wave Prism Borders (Wrapping top face perimeter with fluid ripples)
        drawLiquidWaterBorders (g, topProj[0].screen, topProj[1].screen, topProj[2].screen, colB, colW, colS);
    }

    void draw2DPlanarField (juce::Graphics& g, const Geometry& geom,
                            juce::Colour colB, juce::Colour colW, juce::Colour colS)
    {
        // 2D Classic Planar Barycentric Aura
        juce::Path shellPath;
        buildRoundedTrianglePath (shellPath, geom.vBright, geom.vWarm, geom.vSpacious);

        juce::ColourGradient shellGrad (colB.withAlpha (0.42f), geom.vBright.x, geom.vBright.y,
                                        colW.withAlpha (0.35f), geom.vWarm.x, geom.vWarm.y, false);
        shellGrad.addColour (0.6, colS.withAlpha (0.38f));
        g.setGradientFill (shellGrad);
        g.fillPath (shellPath);

        // Concentric breathing ghost contours
        for (int i = 1; i <= 3; ++i)
        {
            float f = (float) i * 0.25f;
            auto p0 = geom.centre + (geom.vBright - geom.centre) * f;
            auto p1 = geom.centre + (geom.vWarm - geom.centre) * f;
            auto p2 = geom.centre + (geom.vSpacious - geom.centre) * f;

            juce::Path sub;
            buildRoundedTrianglePath (sub, p0, p1, p2);
            g.setColour (colB.withAlpha (0.18f * (1.0f - f)));
            g.strokePath (sub, juce::PathStrokeType (1.0f));
        }

        // Liquid Water-Wave Prism Borders
        drawLiquidWaterBorders (g, geom.vBright, geom.vWarm, geom.vSpacious, colB, colW, colS);
    }

    void drawAtmosphericNebula (juce::Graphics& g, juce::Point<float> centre, float radius,
                                juce::Colour colB, juce::Colour colW, juce::Colour colS, juce::Colour colA)
    {
        float auraR = radius * (1.25f + (spaceSize / 100.0f) * 0.35f + smoothWet * 0.40f);
        auraR *= duckingFactor;

        juce::ColourGradient nebGrad (colB.withAlpha (0.24f * duckingFactor), centre.x, centre.y,
                                      juce::Colour (0x00000000), centre.x + auraR, centre.y, true);
        nebGrad.addColour (0.35, colW.withAlpha (0.18f * duckingFactor));
        nebGrad.addColour (0.65, colS.withAlpha (0.14f * duckingFactor));
        nebGrad.addColour (0.85, colA.withAlpha (0.08f * duckingFactor));
        g.setGradientFill (nebGrad);
        g.fillEllipse (centre.x - auraR, centre.y - auraR * 0.75f, auraR * 2.0f, auraR * 1.5f);
    }

    void drawOrbitalRings (juce::Graphics& g, juce::Point<float> centre, float radius, float scaleY,
                           juce::Colour colB, juce::Colour colW, juce::Colour colS, juce::Colour colA)
    {
        // Stereo Width scales horizontal ellipse expansion
        float wScale = (stereoWidth / 100.0f);

        // 3 Layered Orbital Distance Rings
        const float ringScales[3] = { 0.75f, 1.05f, 1.40f };
        for (int i = 0; i < 3; ++i)
        {
            float rX = radius * ringScales[i] * wScale;
            float rY = radius * ringScales[i] * scaleY * 0.70f;

            // Fade outer rings with decay time & wet energy
            float alpha = 0.14f + (smoothWet * 0.18f) + (decayTime / 30.0f) * 0.10f;
            g.setColour (ProVerbTokens::Colors::border.withAlpha (alpha));
            g.drawEllipse (centre.x - rX, centre.y - rY, rX * 2.0f, rY * 2.0f, 1.0f);
        }

        // Orbiting Satellite Nodes
        auto drawSat = [&](float ang, float rMult, juce::Colour c)
        {
            float rX = radius * rMult * wScale;
            float rY = radius * rMult * scaleY * 0.70f;
            float sx = centre.x + std::cos (ang) * rX;
            float sy = centre.y + std::sin (ang) * rY;
            g.setColour (c.withAlpha (0.30f));
            g.fillEllipse (sx - 6.0f, sy - 6.0f, 12.0f, 12.0f);
            g.setColour (c);
            g.fillEllipse (sx - 2.5f, sy - 2.5f, 5.0f, 5.0f);
        };

        drawSat (yawAngle * 0.8f, 1.05f, colB);
        drawSat (yawAngle * 0.8f + 2.1f, 1.05f, colW);
        drawSat (yawAngle * 0.8f + 4.2f, 1.05f, colS);
        drawSat (-yawAngle * 0.5f + 1.0f, 1.40f, colA);
    }

    void drawTransientRipple (juce::Graphics& g, juce::Point<float> centre, float radius)
    {
        float ripR = radius * transientRingRadius * (stereoWidth / 100.0f);
        float ripY = ripR * 0.72f;

        g.setColour (juce::Colour (0xffffffff).withAlpha (transientRingAlpha * 0.75f));
        g.drawEllipse (centre.x - ripR, centre.y - ripY, ripR * 2.0f, ripY * 2.0f, 2.0f);

        g.setColour (ProVerbTokens::Colors::neonCyan.withAlpha (transientRingAlpha * 0.40f));
        g.drawEllipse (centre.x - ripR * 1.04f, centre.y - ripY * 1.04f, ripR * 2.08f, ripY * 2.08f, 1.2f);
    }

    void drawShimmerParticles (juce::Graphics& g, juce::Point<float> centre, float radius, float scaleY)
    {
        int numParticles = (int) (shimmer * 0.22f + smoothTreble * 15.0f);
        numParticles = juce::jlimit (4, 32, numParticles);

        for (int i = 0; i < numParticles; ++i)
        {
            float pAngle = ((float) i * 1.618f) + shimmerPhase;
            float pDist  = (std::sin ((float) i + shimmerPhase * 0.8f) * 0.5f + 0.5f) * radius * 0.85f;

            float px = centre.x + std::cos (pAngle) * pDist * (stereoWidth / 100.0f);
            float py = centre.y + std::sin (pAngle) * pDist * scaleY * 0.72f;
            float pAlpha = (std::sin (shimmerPhase * 2.5f + (float) i) * 0.5f + 0.5f) * (0.4f + (shimmer / 100.0f) * 0.6f);

            juce::Colour sparkCol = (i % 3 == 0) ? ProVerbTokens::Colors::neonCyan
                                  : (i % 3 == 1) ? ProVerbTokens::Colors::neonAmber
                                  : ProVerbTokens::Colors::neonMint;
            g.setColour (sparkCol.withAlpha (pAlpha));
            g.fillEllipse (px - 1.8f, py - 1.8f, 3.6f, 3.6f);
        }
    }

    void drawVertexNode (juce::Graphics& g, juce::Point<float> pt, const juce::String& label, juce::Colour col, float weight)
    {
        const float nodeR = 8.0f;

        g.setColour (col.withAlpha (0.35f + weight * 0.35f));
        g.fillEllipse (pt.x - 14.0f, pt.y - 14.0f, 28.0f, 28.0f);

        g.setColour (ProVerbTokens::Colors::backgroundMain);
        g.fillEllipse (pt.x - nodeR, pt.y - nodeR, nodeR * 2.0f, nodeR * 2.0f);

        g.setColour (col);
        g.drawEllipse (pt.x - nodeR, pt.y - nodeR, nodeR * 2.0f, nodeR * 2.0f, 2.0f);

        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (pt.x - 2.5f, pt.y - 2.5f, 5.0f, 5.0f);

        g.setFont (ProVerbTokens::Typography::makeLabelFont (9.5f, true));
        g.setColour (ProVerbTokens::Colors::textPrimary);

        if (label == "BRIGHT")
            g.drawText (label, (int) (pt.x - 45.0f), (int) (pt.y - nodeR - 16.0f), 90, 15, juce::Justification::centred);
        else
            g.drawText (label, (int) (pt.x - 45.0f), (int) (pt.y + nodeR + 3.0f), 90, 15, juce::Justification::centred);
    }

    void drawControlPuck (juce::Graphics& g, juce::Point<float> pos)
    {
        const float puckR = 14.0f + (rmsOut * 3.0f) + (smoothTransient * 4.0f);

        g.setColour (juce::Colour (0x66000000));
        g.fillEllipse (pos.x - puckR - 3.5f, pos.y - puckR - 3.5f, (puckR + 3.5f) * 2.0f, (puckR + 3.5f) * 2.0f);

        g.setColour (juce::Colour (0x28ffffff));
        g.fillEllipse (pos.x - puckR - 2.0f, pos.y - puckR - 2.0f, (puckR + 2.0f) * 2.0f, (puckR + 2.0f) * 2.0f);

        g.setColour (juce::Colour (0x35ffffff));
        g.fillEllipse (pos.x - puckR, pos.y - puckR, puckR * 2.0f, puckR * 2.0f);

        g.setColour (juce::Colour (0xffffffff));
        g.drawEllipse (pos.x - puckR, pos.y - puckR, puckR * 2.0f, puckR * 2.0f, 2.6f);
    }

    void drawBottomBar (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        float bottomY = bounds.getBottom() - 22.0f;

        // SPACE: XX% Label
        g.setFont (ProVerbTokens::Typography::makeLabelFont (9.5f, true));
        g.setColour (ProVerbTokens::Colors::neonCyan);
        juce::String spaceText = "SPACE: " + juce::String ((int) spaceSize) + "%";
        g.drawText (spaceText, (int) bounds.getX() + 14, (int) bottomY, 74, 16, juce::Justification::centredLeft);

        // Rainbow Gradient Slider Bar
        auto barRect = getSpaceSliderBarBounds (bounds);
        g.setColour (ProVerbTokens::Colors::backgroundSecondary);
        g.fillRoundedRectangle (barRect, 2.5f);

        juce::ColourGradient rainbowGrad (ProVerbTokens::Colors::neonCyan, barRect.getX(), barRect.getY(),
                                          ProVerbTokens::Colors::neonMint, barRect.getRight(), barRect.getY(), false);
        rainbowGrad.addColour (0.28, ProVerbTokens::Colors::neonViolet);
        rainbowGrad.addColour (0.55, ProVerbTokens::Colors::neonMagenta);
        rainbowGrad.addColour (0.78, ProVerbTokens::Colors::neonAmber);
        g.setGradientFill (rainbowGrad);

        float fillNorm = (spaceSize - 10.0f) / 90.0f;
        auto fillRect = barRect.withWidth (barRect.getWidth() * fillNorm);
        g.fillRoundedRectangle (fillRect, 2.5f);

        // Draggable thumb indicator
        float thumbX = barRect.getX() + barRect.getWidth() * fillNorm;
        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (thumbX - 4.5f, barRect.getCentreY() - 4.5f, 9.0f, 9.0f);
        g.setColour (ProVerbTokens::Colors::neonCyan);
        g.drawEllipse (thumbX - 4.5f, barRect.getCentreY() - 4.5f, 9.0f, 9.0f, 1.2f);

        // Real-Time Stereo Phase Correlation Meter
        const int meterW = 48;
        const int meterH = 5;
        const int meterX = (int) bounds.getRight() - meterW - 14;
        const int meterY = (int) bounds.getBottom() - 17;

        g.setColour (ProVerbTokens::Colors::backgroundSecondary);
        g.fillRoundedRectangle ((float) meterX, (float) meterY, (float) meterW, (float) meterH, 2.0f);
        g.setColour (ProVerbTokens::Colors::border);
        g.drawRoundedRectangle ((float) meterX, (float) meterY, (float) meterW, (float) meterH, 2.0f, 0.8f);

        g.setColour (juce::Colour (0x66ffffff));
        g.drawVerticalLine (meterX + meterW / 2, (float) meterY, (float) (meterY + meterH));

        float pinNorm = juce::jlimit (0.0f, 1.0f, (phaseCorr + 1.0f) * 0.5f);
        float pinX = meterX + pinNorm * (float) (meterW - 4);
        juce::Colour phaseCol = (phaseCorr > 0.35f) ? ProVerbTokens::Colors::neonMint
                              : (phaseCorr >= 0.0f) ? ProVerbTokens::Colors::neonAmber
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

    // Geometry Helpers
    juce::Point<float> getVertexPosition (juce::Point<float> centre, float radius, float scaleY, int vertexIndex) const
    {
        float baseAngle = -juce::MathConstants<float>::halfPi;
        float angle = baseAngle + (float) vertexIndex * (juce::MathConstants<float>::twoPi / 3.0f);

        float vx = centre.x + std::cos (angle) * radius * (stereoWidth / 100.0f);
        float vy = centre.y + std::sin (angle) * radius * scaleY;
        return { vx, vy };
    }

    void buildRoundedTrianglePath (juce::Path& p, juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2) const
    {
        p.clear();
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

    void drawLiquidWaterBorders (juce::Graphics& g,
                                 juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2,
                                 juce::Colour colB, juce::Colour colW, juce::Colour colS)
    {
        std::vector<LiquidPerimeterSample> samples;
        getRoundedTriangleSamples (samples, v0, v1, v2, 84);
        if (samples.empty()) return;

        float t = waterPhase;
        float a1 = 1.8f + smoothBass * 4.2f + smoothTransient * 5.0f;
        float a2 = 1.1f + smoothMid  * 2.4f;
        float a3 = 0.6f + smoothTreble * 2.0f;

        // Idle amplitude scaling when silent (calm, steady fluid motion)
        if (silenceFactor > 0.1f)
        {
            a1 = 1.8f + std::sin (idleBreathPhase * 0.8f) * 0.4f * silenceFactor;
            a2 = 1.1f + std::cos (idleBreathPhase * 0.6f) * 0.3f * silenceFactor;
            a3 = 0.6f;
        }

        juce::Path primaryLiquidPath;
        juce::Path secondaryLiquidPath;
        juce::Path outerAuraPath;

        const float twoPi = juce::MathConstants<float>::twoPi;

        for (size_t idx = 0; idx < samples.size(); ++idx)
        {
            const auto& sp = samples[idx];
            float s = sp.s;

            // Multi-frequency periodic waves (wavenumbers 3, 6, 9 guarantee smooth continuity across all 3 corners)
            float wave1 = std::sin (3.0f * twoPi * s - t * 2.2f);
            float wave2 = std::sin (6.0f * twoPi * s + t * 1.6f + 1.2f);
            float wave3 = std::sin (9.0f * twoPi * s - t * 3.4f + 2.5f);

            float totalDelta = a1 * wave1 + a2 * wave2 + a3 * wave3;
            float secDelta   = a2 * std::cos (6.0f * twoPi * s - t * 1.8f) + 3.5f;
            float auraDelta  = totalDelta + 7.5f;

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
        float wetFactor = juce::jlimit (0.25f, 1.0f, 0.45f + smoothWet * 1.8f + (decayTime / 30.0f) * 0.35f);
        wetFactor *= duckingFactor;

        juce::ColourGradient auraGrad (colB.withAlpha (0.16f * wetFactor), v0.x, v0.y,
                                       colW.withAlpha (0.12f * wetFactor), v1.x, v1.y, false);
        auraGrad.addColour (0.6, colS.withAlpha (0.14f * wetFactor));
        g.setGradientFill (auraGrad);
        g.strokePath (outerAuraPath, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Layer 2: Secondary Exterior Liquid Wave Contour (refractive light ripple)
        juce::ColourGradient secGrad (colS.withAlpha (0.35f * wetFactor), v2.x, v2.y,
                                      colB.withAlpha (0.30f * wetFactor), v0.x, v0.y, false);
        secGrad.addColour (0.5, colW.withAlpha (0.28f * wetFactor));
        g.setGradientFill (secGrad);
        g.strokePath (secondaryLiquidPath, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Layer 3: Primary Illuminated Liquid Glass Border
        juce::ColourGradient primGrad (colB.withAlpha (0.85f * wetFactor), v0.x, v0.y,
                                       colW.withAlpha (0.80f * wetFactor), v1.x, v1.y, false);
        primGrad.addColour (0.58, colS.withAlpha (0.82f * wetFactor));
        g.setGradientFill (primGrad);
        g.strokePath (primaryLiquidPath, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Layer 4: Pure White / Cyan Core Ribbon (water surface reflection line)
        g.setColour (juce::Colour (0xffffffff).withAlpha (0.45f * wetFactor));
        g.strokePath (primaryLiquidPath, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Layer 5: Traveling Specular Liquid Glints (gliding beads of light around perimeter and corners)
        const int numGlints = 3;
        for (int k = 0; k < numGlints; ++k)
        {
            float sGlint = std::fmod (t * 0.08f + (float) k / (float) numGlints, 1.0f);
            size_t idxA = (size_t) (sGlint * (float) samples.size()) % samples.size();
            size_t idxB = (idxA + 1) % samples.size();
            float fract = (sGlint * (float) samples.size()) - (float) idxA;

            auto ptA = samples[idxA].basePos + samples[idxA].normal * (a1 * std::sin (3.0f * twoPi * samples[idxA].s - t * 2.2f));
            auto ptB = samples[idxB].basePos + samples[idxB].normal * (a1 * std::sin (3.0f * twoPi * samples[idxB].s - t * 2.2f));
            auto glintPos = ptA + (ptB - ptA) * fract;

            float glintBrightness = 0.70f + smoothTreble * 0.30f + (shimmer / 100.0f) * 0.25f;
            g.setColour (colB.withAlpha (0.40f * glintBrightness));
            g.fillEllipse (glintPos.x - 5.0f, glintPos.y - 5.0f, 10.0f, 10.0f);
            g.setColour (juce::Colour (0xffffffff).withAlpha (0.92f * glintBrightness));
            g.fillEllipse (glintPos.x - 2.2f, glintPos.y - 2.2f, 4.4f, 4.4f);
        }
    }

    // Interactive Button Bounds
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
        return juce::Rectangle<float> (bounds.getRight() - 100.0f, bounds.getY() + 8.0f, 44.0f, 18.0f);
    }

    juce::Rectangle<float> get2DViewButtonBounds (juce::Rectangle<float> bounds) const
    {
        return juce::Rectangle<float> (bounds.getRight() - 52.0f, bounds.getY() + 8.0f, 44.0f, 18.0f);
    }

    juce::Rectangle<float> getSpaceSliderBarBounds (juce::Rectangle<float> bounds) const
    {
        float barX = bounds.getX() + 92.0f;
        float barW = bounds.getWidth() - 250.0f;
        float barY = bounds.getBottom() - 17.0f;
        return juce::Rectangle<float> (barX, barY, barW, 6.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Check Mode Buttons: LIVE / SUBTLE / FREEZE
        if (getLiveModeButtonBounds (bounds).contains (e.position))
        {
            prismMode = PrismMode::Live;
            repaint();
            return;
        }
        if (getSubtleModeButtonBounds (bounds).contains (e.position))
        {
            prismMode = PrismMode::Subtle;
            repaint();
            return;
        }
        if (getFreezeModeButtonBounds (bounds).contains (e.position))
        {
            prismMode = PrismMode::Freeze;
            repaint();
            return;
        }

        // 2. Check 3D / 2D View Buttons
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

        // 3. Check Bottom Space Slider Bar Click / Drag
        auto sliderBarRect = getSpaceSliderBarBounds (bounds);
        if (sliderBarRect.expanded (6.0f, 8.0f).contains (e.position))
        {
            isDraggingSpaceSlider = true;
            updateSpaceSliderFromMouse (e.position.x, sliderBarRect);
            return;
        }

        // 4. Check Central Puck Drag
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
        else if (getLiveModeButtonBounds (bounds).contains (e.position)
              || getSubtleModeButtonBounds (bounds).contains (e.position)
              || getFreezeModeButtonBounds (bounds).contains (e.position)
              || get3DViewButtonBounds (bounds).contains (e.position)
              || get2DViewButtonBounds (bounds).contains (e.position))
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void updateSpaceSliderFromMouse (float mouseX, juce::Rectangle<float> barRect)
    {
        float norm = juce::jlimit (0.0f, 1.0f, (mouseX - barRect.getX()) / barRect.getWidth());
        spaceSize = 10.0f + norm * 90.0f;
        if (onSpaceSizeChanged)
            onSpaceSizeChanged (spaceSize);
        repaint();
    }

    void updatePuckFromMouse (juce::Point<float> mousePos, juce::Point<float> v0, juce::Point<float> v1, juce::Point<float> v2)
    {
        float denom = (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y);
        if (std::abs (denom) < 0.0001f) return;

        float w0 = ((v1.y - v2.y) * (mousePos.x - v2.x) + (v2.x - v1.x) * (mousePos.y - v2.y)) / denom;
        float w1 = ((v2.y - v0.y) * (mousePos.x - v2.x) + (v0.x - v2.x) * (mousePos.y - v2.y)) / denom;
        float w2 = 1.0f - w0 - w1;

        w0 = juce::jlimit (0.05f, 0.90f, w0);
        w1 = juce::jlimit (0.05f, 0.90f, w1);
        w2 = juce::jlimit (0.05f, 0.90f, w2);

        float sum = w0 + w1 + w2;
        weightBright = w0 / sum;
        weightWarm   = w1 / sum;
        weightSpace  = w2 / sum;

        if (onAcousticBalanceChanged)
            onAcousticBalanceChanged (weightBright, weightWarm, weightSpace);

        repaint();
    }

private:
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

    // Dynamic transient ripple
    float transientRingRadius { 0.0f };
    float transientRingAlpha { 0.0f };

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
    float yawAngle { 0.0f };
    float idleBreathPhase { 0.0f };
    float shimmerPhase { 0.0f };
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
