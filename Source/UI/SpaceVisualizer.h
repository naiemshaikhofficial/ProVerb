#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

class SpaceVisualizer : public juce::Component
{
public:
    SpaceVisualizer() = default;

    void updateAllParameters (float newRmsIn, float newRmsOut,
                              float sizePercent, float decaySec,
                              float diffusionPercent, float dampingPercent,
                              float predelayMs, float shimmerPercent,
                              float newPhaseCorr = 1.0f)
    {
        rmsIn = newRmsIn;
        rmsOut = newRmsOut;
        spaceSize = sizePercent;
        decayTime = decaySec;
        diffusion = diffusionPercent;
        damping = dampingPercent;
        predelay = predelayMs;
        shimmer = shimmerPercent;
        phaseCorr = newPhaseCorr;
    }

    void setAnalyzing (bool analyzing, float progress = 0.0f)
    {
        isAnalyzing = analyzing;
        analyzeProgress = progress;
    }

    void advanceAnimation (float deltaPhase)
    {
        // Smooth rotation speed
        rotationAngle += deltaPhase * 0.5f;
        if (rotationAngle > juce::MathConstants<float>::twoPi)
            rotationAngle -= juce::MathConstants<float>::twoPi;

        // Sparkle animation for shimmer
        shimmerPhase += deltaPhase * 1.5f;
        if (shimmerPhase > juce::MathConstants<float>::twoPi)
            shimmerPhase -= juce::MathConstants<float>::twoPi;

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Futuristic Deep Obsidian Well
        g.setColour (juce::Colour (0xff06070a));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (juce::Colour (0xff181a24));
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

        // Minimalist Cyber HUD Badge (Top Left)
        g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
        g.setColour (juce::Colour (0x5538bdf8));
        g.drawText ("[ 3D ACOUSTIC MATRIX ]", (int) bounds.getX() + 10, (int) bounds.getY() + 6, 140, 12, juce::Justification::left);

        auto centre = bounds.getCentre();

        // =========================================================================
        // PARAMETER-DRIVEN GEOMETRY (Real-time visual feedback)
        // =========================================================================
        
        // 1. SIZE -> Radius / Circumference
        float minRadiusX = bounds.getWidth() * 0.18f;
        float maxRadiusX = bounds.getWidth() * 0.44f;
        float radiusX = juce::jmap (spaceSize, 10.0f, 100.0f, minRadiusX, maxRadiusX);
        float radiusY = radiusX * 0.35f; // 3D Perspective tilt

        // Audio reactivity expands the circumference slightly on sound hits
        radiusX += rmsOut * 18.0f;
        radiusY += rmsOut * 6.0f;

        // 2. DECAY -> Cylinder Height
        float minHeight = bounds.getHeight() * 0.25f;
        float maxHeight = bounds.getHeight() * 0.65f;
        float normalizedDecay = juce::jlimit (0.0f, 1.0f, std::log10 (decayTime * 3.33f + 1.0f) * 0.5f);
        float cylinderHeight = juce::jmap (normalizedDecay, 0.0f, 1.0f, minHeight, maxHeight);

        // Pre-delay introduces a slight vertical floating lift
        float predelayLift = juce::jlimit (0.0f, 10.0f, predelay * 0.035f);
        float topCenterY = (centre.y - cylinderHeight * 0.5f) - predelayLift;
        float bottomCenterY = (centre.y + cylinderHeight * 0.5f) - predelayLift;

        // 3. DIFFUSION -> Number of Vertical Wireframe Ribs
        int numRibs = (int) juce::jmap (diffusion, 0.0f, 100.0f, 10.0f, 28.0f);
        if (numRibs % 2 != 0) numRibs++; // Keep even for symmetry

        // 4. DAMPING -> Color Temperature & Bloom
        float dampFactor = damping / 100.0f;
        juce::Colour wireframeFrontCol = juce::Colour (0xff00f0ff).interpolatedWith (juce::Colour (0xffe2e8f0), 1.0f - dampFactor * 0.5f);
        juce::Colour wireframeBackCol  = juce::Colour (0x2800f0ff).interpolatedWith (juce::Colour (0x1294a3b8), dampFactor);

        // =========================================================================
        // NEOVERB-STYLE COLORFUL ACOUSTIC AURA & MULTI-GRADIENT NEBULA
        // =========================================================================

        // 1. Top Lobe Glow: Pure Glowing White (Reflections / Early Space)
        float whiteIntensity = juce::jlimit (0.15f, 0.85f, 0.25f + (spaceSize / 100.0f) * 0.4f + rmsOut * 0.6f);
        juce::ColourGradient whiteAura (juce::Colour (0xffffffff).withAlpha (whiteIntensity * 0.28f), centre.x, topCenterY,
                                        juce::Colour (0x00000000), centre.x, centre.y + cylinderHeight * 0.4f, true);
        g.setGradientFill (whiteAura);
        g.fillEllipse (centre.x - radiusX * 1.05f, topCenterY - radiusY * 1.5f, radiusX * 2.1f, cylinderHeight * 0.9f);

        // 2. Bottom-Left Lobe Glow: Electric Violet / Magenta (Room Depth & Body)
        float violetIntensity = juce::jlimit (0.15f, 0.80f, 0.20f + (diffusion / 100.0f) * 0.35f + rmsOut * 0.5f);
        float blX = centre.x - radiusX * 0.6f;
        float blY = bottomCenterY + radiusY * 0.3f;
        juce::ColourGradient violetAura (juce::Colour (0xff8b5cf6).withAlpha (violetIntensity * 0.35f), blX, blY,
                                         juce::Colour (0x00000000), centre.x, centre.y - cylinderHeight * 0.3f, true);
        g.setGradientFill (violetAura);
        g.fillEllipse (blX - radiusX * 0.8f, blY - cylinderHeight * 0.5f, radiusX * 1.6f, cylinderHeight * 0.9f);

        // 3. Bottom-Right Lobe Glow: Warm Coral / Rose / Amber (Decay Tail & Shimmer)
        float coralIntensity = juce::jlimit (0.15f, 0.85f, 0.20f + normalizedDecay * 0.4f + rmsOut * 0.5f);
        float brX = centre.x + radiusX * 0.6f;
        float brY = bottomCenterY + radiusY * 0.3f;
        juce::ColourGradient coralAura (juce::Colour (0xffff5e7e).withAlpha (coralIntensity * 0.35f), brX, brY,
                                        juce::Colour (0x00000000), centre.x, centre.y - cylinderHeight * 0.3f, true);
        g.setGradientFill (coralAura);
        g.fillEllipse (brX - radiusX * 0.8f, brY - cylinderHeight * 0.5f, radiusX * 1.6f, cylinderHeight * 0.9f);

        // 4. Concentric Pulsing Sonar Resonance Waves (Neoverb Ripple Waves)
        for (int w = 0; w < 3; ++w)
        {
            float wavePhase = std::fmod (shimmerPhase * 0.25f + (float) w * 0.333f, 1.0f);
            float waveRadX = radiusX * (0.35f + wavePhase * 0.80f);
            float waveRadY = waveRadX * 0.38f;
            float waveAlpha = (1.0f - wavePhase) * (0.14f + rmsOut * 0.40f);
            
            juce::Colour waveCol = (w == 0 ? juce::Colour (0xff00f0ff) : (w == 1 ? juce::Colour (0xffa855f7) : juce::Colour (0xffff5e7e)));
            g.setColour (waveCol.withAlpha (waveAlpha));
            g.drawEllipse (centre.x - waveRadX, centre.y - waveRadY, waveRadX * 2.0f, waveRadY * 2.0f, 1.2f);
        }

        // =========================================================================
        // DRAW 3D WIREFRAME CYLINDER WITH HOLOGRAPHIC CROSS-SECTIONS
        // =========================================================================

        // Draw Back Ribs (Perspective depth: dark and thin)
        for (int i = 0; i < numRibs; ++i)
        {
            float theta = ((float) i / (float) numRibs) * juce::MathConstants<float>::twoPi + rotationAngle;
            float cosT = std::cos (theta);
            float sinT = std::sin (theta);

            if (sinT < 0.0f) // Back side
            {
                float x = centre.x + cosT * radiusX;
                float yTop = topCenterY + sinT * radiusY;
                float yBot = bottomCenterY + sinT * radiusY;

                g.setColour (wireframeBackCol);
                g.drawLine (x, yTop, x, yBot, 1.0f);
            }
        }

        // Draw Back Ellipses
        g.setColour (wireframeBackCol);
        g.drawEllipse (centre.x - radiusX, topCenterY - radiusY, radiusX * 2.0f, radiusY * 2.0f, 1.0f);
        g.drawEllipse (centre.x - radiusX, bottomCenterY - radiusY, radiusX * 2.0f, radiusY * 2.0f, 1.0f);

        // Holographic Intermediate Horizontal Slices (Back Arcs)
        float mid1Y = juce::jmap (0.33f, topCenterY, bottomCenterY);
        float mid2Y = juce::jmap (0.66f, topCenterY, bottomCenterY);
        juce::Path mid1Back, mid2Back;
        mid1Back.addCentredArc (centre.x, mid1Y, radiusX, radiusY, 0.0f, juce::MathConstants<float>::pi, juce::MathConstants<float>::twoPi, true);
        mid2Back.addCentredArc (centre.x, mid2Y, radiusX, radiusY, 0.0f, juce::MathConstants<float>::pi, juce::MathConstants<float>::twoPi, true);
        g.setColour (wireframeBackCol.withAlpha (0.15f));
        g.strokePath (mid1Back, juce::PathStrokeType (0.8f));
        g.strokePath (mid2Back, juce::PathStrokeType (0.8f));

        // 5. SHIMMER -> Floating Star Particles inside the space
        if (shimmer > 1.0f)
        {
            int numParticles = (int) (shimmer * 0.12f);
            for (int p = 0; p < numParticles; ++p)
            {
                float pAngle = ((float) p * 1.35f) + shimmerPhase;
                float pDistX = (std::cos (pAngle * 1.4f) * radiusX * 0.65f);
                float pDistY = (std::sin (pAngle * 2.1f) * cylinderHeight * 0.35f);
                float pAlpha = (std::sin (shimmerPhase * 2.0f + (float) p) * 0.5f + 0.5f) * (shimmer / 100.0f);

                g.setColour (juce::Colour (0xff00f0ff).withAlpha (pAlpha));
                g.fillEllipse (centre.x + pDistX - 1.5f, centre.y + pDistY - 1.5f, 3.0f, 3.0f);
            }
        }

        // Draw Front Ribs (Perspective foreground: glowing cyan & laser white)
        for (int i = 0; i < numRibs; ++i)
        {
            float theta = ((float) i / (float) numRibs) * juce::MathConstants<float>::twoPi + rotationAngle;
            float cosT = std::cos (theta);
            float sinT = std::sin (theta);

            if (sinT >= 0.0f) // Front side
            {
                float x = centre.x + cosT * radiusX;
                float yTop = topCenterY + sinT * radiusY;
                float yBot = bottomCenterY + sinT * radiusY;

                // Outer soft neon bloom
                g.setColour (wireframeFrontCol.withAlpha (0.24f + rmsOut * 0.30f));
                g.drawLine (x, yTop, x, yBot, 3.0f);

                // Bright core line
                g.setColour (wireframeFrontCol.withAlpha (0.95f));
                g.drawLine (x, yTop, x, yBot, 1.2f);
            }
        }

        // Holographic Intermediate Horizontal Slices (Front Arcs)
        juce::Path mid1Front, mid2Front;
        mid1Front.addCentredArc (centre.x, mid1Y, radiusX, radiusY, 0.0f, 0.0f, juce::MathConstants<float>::pi, true);
        mid2Front.addCentredArc (centre.x, mid2Y, radiusX, radiusY, 0.0f, 0.0f, juce::MathConstants<float>::pi, true);
        g.setColour (wireframeFrontCol.withAlpha (0.35f));
        g.strokePath (mid1Front, juce::PathStrokeType (1.0f));
        g.strokePath (mid2Front, juce::PathStrokeType (1.0f));

        // Front Rim Rings (Glowing Cyber Rim)
        juce::Path topPath;
        topPath.addCentredArc (centre.x, topCenterY, radiusX, radiusY, 0.0f, 0.0f, juce::MathConstants<float>::pi, true);
        g.setColour (wireframeFrontCol.withAlpha (0.35f));
        g.strokePath (topPath, juce::PathStrokeType (4.0f));
        g.setColour (wireframeFrontCol);
        g.strokePath (topPath, juce::PathStrokeType (1.8f));

        juce::Path bottomPath;
        bottomPath.addCentredArc (centre.x, bottomCenterY, radiusX, radiusY, 0.0f, 0.0f, juce::MathConstants<float>::pi, true);
        g.setColour (wireframeFrontCol.withAlpha (0.35f));
        g.strokePath (bottomPath, juce::PathStrokeType (4.0f));
        g.setColour (wireframeFrontCol);
        g.strokePath (bottomPath, juce::PathStrokeType (1.8f));

        // 6. Central Holographic Control Orb (Acoustic Singularity Node)
        float orbRadius = 7.0f + (rmsOut * 14.0f);
        // Outer halo
        g.setColour (juce::Colour (0x2800f0ff));
        g.fillEllipse (centre.x - orbRadius * 1.8f, centre.y - orbRadius * 1.8f, orbRadius * 3.6f, orbRadius * 3.6f);
        // Translucent dark glass sphere
        g.setColour (juce::Colour (0xd007090e));
        g.fillEllipse (centre.x - orbRadius, centre.y - orbRadius, orbRadius * 2.0f, orbRadius * 2.0f);
        // Glowing neon rim ring
        g.setColour (juce::Colour (0xff00f0ff));
        g.drawEllipse (centre.x - orbRadius, centre.y - orbRadius, orbRadius * 2.0f, orbRadius * 2.0f, 1.5f);
        // Inner jewel nucleus
        float jewelR = 3.0f + (rmsIn * 5.0f);
        g.setColour (juce::Colour (0xffffffff));
        g.fillEllipse (centre.x - jewelR, centre.y - jewelR, jewelR * 2.0f, jewelR * 2.0f);

        // 7. Interactive Feedback Badges at Bottom (Futuristic Cyber Typography)
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.setColour (juce::Colour (0xff38bdf8));
        g.drawText ("SPACE: " + juce::String ((int) spaceSize) + "%", (int) bounds.getX() + 10, (int) bounds.getBottom() - 18, 75, 14, juce::Justification::left);

        // Real-Time Stereo Phase Correlation Meter (Mono Compatibility Indicator)
        const int meterW = 56;
        const int meterH = 4;
        const int meterX = (int) bounds.getRight() - meterW - 8;
        const int meterY = (int) bounds.getBottom() - 12;

        g.setColour (juce::Colour (0xff0c0d12));
        g.fillRoundedRectangle ((float) meterX, (float) meterY, (float) meterW, (float) meterH, 2.0f);
        g.setColour (juce::Colour (0xff1e222e));
        g.drawRoundedRectangle ((float) meterX, (float) meterY, (float) meterW, (float) meterH, 2.0f, 0.8f);

        // Center tick
        g.setColour (juce::Colour (0x66ffffff));
        g.drawVerticalLine (meterX + meterW / 2, (float) meterY, (float) (meterY + meterH));

        float pinNorm = juce::jlimit (0.0f, 1.0f, (phaseCorr + 1.0f) * 0.5f);
        float pinX = meterX + pinNorm * (float) (meterW - 4);
        juce::Colour phaseCol = (phaseCorr > 0.35f) ? juce::Colour (0xff10b981)
                              : (phaseCorr >= 0.0f) ? juce::Colour (0xfff59e0b)
                              : juce::Colour (0xffef4444);

        g.setColour (phaseCol);
        g.fillRoundedRectangle (pinX, (float) meterY - 1.0f, 3.5f, (float) (meterH + 2), 1.5f);

        juce::String phaseText = juce::String ("PHASE: ") + (phaseCorr >= 0.0f ? "+" : "") + juce::String (phaseCorr, 2);
        g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
        g.setColour (phaseCol.withAlpha (0.95f));
        g.drawText (phaseText, meterX - 74, (int) bounds.getBottom() - 18, 70, 14, juce::Justification::right);

        // 8. Auto-Learn Analysis Radar & Laser Scan
        if (isAnalyzing)
        {
            // Vertical scanning laser line
            float scanY = juce::jmap (analyzeProgress, 0.0f, 1.0f, topCenterY - radiusY, bottomCenterY + radiusY);

            // Laser beam glow
            juce::ColourGradient beamGrad (juce::Colour (0x00ffffff), centre.x - radiusX * 1.1f, scanY,
                                           juce::Colour (0xffffffff), centre.x, scanY, true);
            g.setGradientFill (beamGrad);
            g.fillRect (centre.x - radiusX * 1.1f, scanY - 2.0f, radiusX * 2.2f, 4.0f);

            // Pulsing target halo
            float pulseRadius = radiusX * (0.3f + 0.7f * std::fmod (analyzeProgress * 3.0f, 1.0f));
            g.setColour (juce::Colour (0x55ffffff));
            g.drawEllipse (centre.x - pulseRadius, centre.y - pulseRadius * 0.35f, pulseRadius * 2.0f, pulseRadius * 0.7f, 1.5f);

            // Scanning banner overlay
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.setColour (juce::Colour (0xffffffff));
            g.drawText ("AI ACOUSTIC SCAN: " + juce::String ((int)(analyzeProgress * 100.0f)) + "%",
                        (int) bounds.getX(), (int) bounds.getY() + 8, (int) bounds.getWidth(), 18, juce::Justification::centred);
        }
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

    float rotationAngle = 0.0f;
    float shimmerPhase = 0.0f;

    bool isAnalyzing = false;
    float analyzeProgress = 0.0f;
};
