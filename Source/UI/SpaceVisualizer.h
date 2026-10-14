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

        // 1. Dark Studio Chassis Well
        g.setColour (juce::Colour (0xff0d0e11));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (juce::Colour (0xff1f2127));
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

        auto centre = bounds.getCentre();

        // =========================================================================
        // PARAMETER-DRIVEN GEOMETRY (Real-time visual feedback)
        // =========================================================================
        
        // 1. SIZE -> Radius / Circumference (Mota / Patla)
        // Maps 10% to 100% smoothly to width percentage
        float minRadiusX = bounds.getWidth() * 0.18f;
        float maxRadiusX = bounds.getWidth() * 0.44f;
        float radiusX = juce::jmap (spaceSize, 10.0f, 100.0f, minRadiusX, maxRadiusX);
        float radiusY = radiusX * 0.35f; // 3D Perspective tilt

        // Audio reactivity expands the circumference slightly on sound hits
        radiusX += rmsOut * 18.0f;
        radiusY += rmsOut * 6.0f;

        // 2. DECAY -> Cylinder Height (Lamba / Chhota)
        // Maps 0.1s to 30s decay time to vertical span
        float minHeight = bounds.getHeight() * 0.25f;
        float maxHeight = bounds.getHeight() * 0.65f;
        float normalizedDecay = juce::jlimit (0.0f, 1.0f, std::log10 (decayTime * 3.33f + 1.0f) * 0.5f);
        float cylinderHeight = juce::jmap (normalizedDecay, 0.0f, 1.0f, minHeight, maxHeight);

        // Pre-delay introduces a slight vertical floating lift
        float predelayLift = juce::jlimit (0.0f, 10.0f, predelay * 0.035f);
        float topCenterY = (centre.y - cylinderHeight * 0.5f) - predelayLift;
        float bottomCenterY = (centre.y + cylinderHeight * 0.5f) - predelayLift;

        // 3. DIFFUSION -> Number of Vertical Wireframe Ribs (Sparser vs Denser)
        // 0% diffusion = 8 ribs, 100% diffusion = 28 ribs
        int numRibs = (int) juce::jmap (diffusion, 0.0f, 100.0f, 10.0f, 28.0f);
        if (numRibs % 2 != 0) numRibs++; // Keep even for symmetry

        // 4. DAMPING -> Color Temperature & Bloom
        // Low damping (0%) = Crisp Electric Cyan-Blue & White (Bright air)
        // High damping (100%) = Deep Muted Navy-Indigo (Warm air absorption)
        float dampFactor = damping / 100.0f;
        juce::Colour wireframeFrontCol = juce::Colour (0xffffffff).interpolatedWith (juce::Colour (0xffe2e8f0), dampFactor);
        juce::Colour wireframeBackCol  = juce::Colour (0x33ffffff).interpolatedWith (juce::Colour (0x1594a3b8), dampFactor);

        // =========================================================================
        // NEOVERB-STYLE COLORFUL ACOUSTIC AURA & MULTI-GRADIENT NEBULA
        // =========================================================================

        // 1. Top Lobe Glow: Pure Glowing White (Reflections / Early Space)
        float whiteIntensity = juce::jlimit (0.15f, 0.85f, 0.25f + (spaceSize / 100.0f) * 0.4f + rmsOut * 0.6f);
        juce::ColourGradient whiteAura (juce::Colour (0xffffffff).withAlpha (whiteIntensity * 0.32f), centre.x, topCenterY,
                                        juce::Colour (0x00000000), centre.x, centre.y + cylinderHeight * 0.4f, true);
        g.setGradientFill (whiteAura);
        g.fillEllipse (centre.x - radiusX * 1.05f, topCenterY - radiusY * 1.5f, radiusX * 2.1f, cylinderHeight * 0.9f);

        // 2. Bottom-Left Lobe Glow: Electric Violet / Magenta (Room Depth & Body)
        float violetIntensity = juce::jlimit (0.15f, 0.80f, 0.20f + (diffusion / 100.0f) * 0.35f + rmsOut * 0.5f);
        float blX = centre.x - radiusX * 0.6f;
        float blY = bottomCenterY + radiusY * 0.3f;
        juce::ColourGradient violetAura (juce::Colour (0xff8b5cf6).withAlpha (violetIntensity * 0.40f), blX, blY,
                                         juce::Colour (0x00000000), centre.x, centre.y - cylinderHeight * 0.3f, true);
        g.setGradientFill (violetAura);
        g.fillEllipse (blX - radiusX * 0.8f, blY - cylinderHeight * 0.5f, radiusX * 1.6f, cylinderHeight * 0.9f);

        // 3. Bottom-Right Lobe Glow: Warm Coral / Rose / Amber (Decay Tail & Shimmer)
        float coralIntensity = juce::jlimit (0.15f, 0.85f, 0.20f + normalizedDecay * 0.4f + rmsOut * 0.5f);
        float brX = centre.x + radiusX * 0.6f;
        float brY = bottomCenterY + radiusY * 0.3f;
        juce::ColourGradient coralAura (juce::Colour (0xffff5e7e).withAlpha (coralIntensity * 0.40f), brX, brY,
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
            
            juce::Colour waveCol = (w == 0 ? juce::Colour (0xffffffff) : (w == 1 ? juce::Colour (0xffa855f7) : juce::Colour (0xffff5e7e)));
            g.setColour (waveCol.withAlpha (waveAlpha));
            g.drawEllipse (centre.x - waveRadX, centre.y - waveRadY, waveRadX * 2.0f, waveRadY * 2.0f, 1.4f);
        }

        // =========================================================================
        // DRAW 3D WIREFRAME CYLINDER
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

                g.setColour (juce::Colour (0xffe0f2fe).withAlpha (pAlpha));
                g.fillEllipse (centre.x + pDistX - 1.5f, centre.y + pDistY - 1.5f, 3.0f, 3.0f);
            }
        }

        // Draw Front Ribs (Perspective foreground: bright with neon bloom)
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
                g.setColour (wireframeFrontCol.withAlpha (0.28f + rmsOut * 0.35f));
                g.drawLine (x, yTop, x, yBot, 3.2f);

                // Bright core line
                g.setColour (wireframeFrontCol.withAlpha (0.95f));
                g.drawLine (x, yTop, x, yBot, 1.4f);
            }
        }

        // Front Rim Rings (Thick glowing border)
        juce::Path topPath;
        topPath.addCentredArc (centre.x, topCenterY, radiusX, radiusY, 0.0f, 0.0f, juce::MathConstants<float>::pi, true);
        g.setColour (wireframeFrontCol);
        g.strokePath (topPath, juce::PathStrokeType (2.2f));

        juce::Path bottomPath;
        bottomPath.addCentredArc (centre.x, bottomCenterY, radiusX, radiusY, 0.0f, 0.0f, juce::MathConstants<float>::pi, true);
        g.strokePath (bottomPath, juce::PathStrokeType (2.2f));

        // 6. Central Holographic Control Orb (Neoverb Space Node)
        float orbRadius = 8.0f + (rmsOut * 16.0f);
        // Outer halo
        g.setColour (juce::Colour (0x33ffffff));
        g.fillEllipse (centre.x - orbRadius * 1.8f, centre.y - orbRadius * 1.8f, orbRadius * 3.6f, orbRadius * 3.6f);
        // Translucent dark glass sphere
        g.setColour (juce::Colour (0xbb0d1117));
        g.fillEllipse (centre.x - orbRadius, centre.y - orbRadius, orbRadius * 2.0f, orbRadius * 2.0f);
        // Glowing neon rim ring (Pure White)
        g.setColour (juce::Colour (0xffffffff));
        g.drawEllipse (centre.x - orbRadius, centre.y - orbRadius, orbRadius * 2.0f, orbRadius * 2.0f, 1.8f);
        // Inner jewel nucleus
        float jewelR = 3.5f + (rmsIn * 6.0f);
        g.setColour (juce::Colour (0xffffffff).withAlpha (0.95f));
        g.fillEllipse (centre.x - jewelR, centre.y - jewelR, jewelR * 2.0f, jewelR * 2.0f);

        // 7. Interactive Feedback Badges at Bottom (Cleanly separated non-overlapping zones)
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xff94a3b8));
        g.drawText ("SPACE: " + juce::String ((int) spaceSize) + "%", (int) bounds.getX() + 10, (int) bounds.getBottom() - 18, 75, 14, juce::Justification::left);

        // Real-Time Stereo Phase Correlation Meter (Mono Compatibility Indicator)
        const int meterW = 56;
        const int meterH = 5;
        const int meterX = (int) bounds.getRight() - meterW - 8;
        const int meterY = (int) bounds.getBottom() - 13;

        g.setColour (juce::Colour (0xff14161d));
        g.fillRoundedRectangle ((float) meterX, (float) meterY, (float) meterW, (float) meterH, 2.5f);
        g.setColour (juce::Colour (0x33ffffff));
        g.drawRoundedRectangle ((float) meterX, (float) meterY, (float) meterW, (float) meterH, 2.5f, 0.8f);

        // Center tick
        g.setColour (juce::Colour (0x55ffffff));
        g.drawVerticalLine (meterX + meterW / 2, (float) meterY, (float) (meterY + meterH));

        float pinNorm = juce::jlimit (0.0f, 1.0f, (phaseCorr + 1.0f) * 0.5f);
        float pinX = meterX + pinNorm * (float) (meterW - 4);
        juce::Colour phaseCol = (phaseCorr > 0.35f) ? juce::Colour (0xff10b981)
                              : (phaseCorr >= 0.0f) ? juce::Colour (0xfff59e0b)
                              : juce::Colour (0xffef4444);

        g.setColour (phaseCol);
        g.fillRoundedRectangle (pinX, (float) meterY - 1.0f, 4.0f, (float) (meterH + 2), 2.0f);

        juce::String phaseText = juce::String ("PHASE: ") + (phaseCorr >= 0.0f ? "+" : "") + juce::String (phaseCorr, 2);
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
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
