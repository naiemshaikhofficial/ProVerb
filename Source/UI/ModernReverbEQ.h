#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include <algorithm>
#include <vector>

/**
 * ModernReverbEQ — High-Detail Interactive Reverb Filter & Real-Time Spectrum Analyzer
 * ProducerToy — ProVerb Smart Algorithmic Reverb
 *
 * Designed to match the ProducerToy dark theme (--epic-bg: #121212, --epic-card: #202020)
 * and the sleek Equinox reference architecture with interactive 'I', 'E', 'R' nodes,
 * filter shape badges, frequency grid, and real-time organic acoustic spectrum visualization.
 */
class ModernReverbEQ : public juce::Component
{
public:
    enum class FilterShape
    {
        HighPass,
        LowShelf,
        Bell,
        Notch,
        HighCut
    };

    ModernReverbEQ()
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    std::function<void(float)> onLowCutChanged;
    std::function<void(float)> onHighCutChanged;
    std::function<void(float, float)> onMidEqChanged; // freq, gainDb

    void updateParameters (float lowCutHz, float highCutHz, float audioRms)
    {
        if (activeDragHandle != Handle::LowCut)
            currentLowCut = juce::jlimit (20.0f, 2000.0f, lowCutHz);

        if (activeDragHandle != Handle::HighCut)
            currentHighCut = juce::jlimit (500.0f, 20000.0f, highCutHz);

        rmsLevel = juce::jlimit (0.0f, 1.0f, audioRms);
        repaint();
    }

    void setAnalyzing (bool analyzing, float progress = 0.0f)
    {
        isAnalyzing = analyzing;
        analyzeProgress = juce::jlimit (0.0f, 1.0f, progress);
        repaint();
    }

    void advanceAnimation (float delta)
    {
        spectrumPhase += delta * 2.5f;
        if (spectrumPhase > juce::MathConstants<float>::twoPi * 10.0f)
            spectrumPhase -= juce::MathConstants<float>::twoPi * 10.0f;
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Chassis Background (Deep Obsidian Black #08090c)
        g.setColour (juce::Colour (0xff08090c));
        g.fillRoundedRectangle (bounds, 6.0f);

        // Subtle Cyber Border (#1c1f2b)
        g.setColour (juce::Colour (0xff1c1f2b));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

        auto plotArea = getPlotArea();
        const float plotW = plotArea.getWidth();
        const float plotH = plotArea.getHeight();
        const float plotX = plotArea.getX();
        const float plotY = plotArea.getY();

        if (plotW <= 20.0f || plotH <= 20.0f)
            return;

        // Dark Display Inset Well (Deep OLED #0a0b0f)
        g.setColour (juce::Colour (0xff0a0b0f));
        g.fillRoundedRectangle (plotArea, 4.0f);
        g.setColour (juce::Colour (0xff181a24));
        g.drawRoundedRectangle (plotArea, 4.0f, 1.0f);

        // 2. Frequency Grid Lines & Bottom Labels
        const struct FreqMarker { float freq; const char* label; } markers[] = {
            { 20.0f, "20" },
            { 50.0f, "50" },
            { 100.0f, "100" },
            { 250.0f, "250" },
            { 500.0f, "500" },
            { 1000.0f, "1k" },
            { 2500.0f, "2.5k" },
            { 5000.0f, "5k" },
            { 10000.0f, "10k" },
            { 20000.0f, "20k" }
        };

        g.setFont (juce::FontOptions (8.5f, juce::Font::plain));
        for (const auto& m : markers)
        {
            float x = freqToX (m.freq, plotW) + plotX;
            g.setColour (juce::Colour (0x12ffffff));
            g.drawVerticalLine ((int) x, plotY, plotY + plotH);

            g.setColour (juce::Colour (0xff606470));
            g.drawText (m.label, (int) (x - 16.0f), (int) (plotY + plotH + 2.0f), 32, 12, juce::Justification::centred);
        }

        // 3. dB Horizontal Grid Lines (+12dB, +6dB, 0dB, -6dB, -18dB)
        const struct DbMarker { float db; const char* label; } dbMarkers[] = {
            { 12.0f, "+12" },
            { 6.0f, "+6" },
            { 0.0f, "0 dB" },
            { -6.0f, "-6" },
            { -18.0f, "-18" }
        };

        for (const auto& dm : dbMarkers)
        {
            float y = dbToY (dm.db, plotH) + plotY;
            g.setColour (dm.db == 0.0f ? juce::Colour (0x33ffffff) : juce::Colour (0x0effffff));
            g.drawHorizontalLine ((int) y, plotX, plotX + plotW);

            g.setColour (juce::Colour (0xff525562));
            g.drawText (dm.label, (int) (plotX + plotW - 28.0f), (int) (y - 6.0f), 24, 12, juce::Justification::right);
        }

        // 4. Multi-Layer Real-Time Spectrum Visualizer (Teal / Cyan organic acoustic energy)
        if (rmsLevel > 0.002f)
        {
            juce::Path spectrumPath;
            spectrumPath.startNewSubPath (plotX, plotY + plotH);

            for (float px = 0.0f; px <= plotW; px += 3.0f)
            {
                float freq = xToFreq (px, plotW);
                // Acoustic reverberant spectral curve
                float freqWeight = std::sin (std::log10 (freq / 20.0f) / std::log10 (1000.0f) * juce::MathConstants<float>::pi);
                freqWeight = juce::jmax (0.05f, freqWeight);

                // Add organic ripple dynamics
                float ripple1 = std::sin (freq * 0.02f + spectrumPhase) * 0.35f;
                float ripple2 = std::cos (freq * 0.05f - spectrumPhase * 1.4f) * 0.22f;
                float ripple3 = std::sin (freq * 0.09f + spectrumPhase * 2.1f) * 0.15f;

                float amp = (rmsLevel * 1.8f + (ripple1 + ripple2 + ripple3) * rmsLevel) * freqWeight;
                float specH = juce::jlimit (0.0f, plotH * 0.85f, amp * plotH * 0.82f);
                spectrumPath.lineTo (plotX + px, plotY + plotH - specH);
            }
            spectrumPath.lineTo (plotX + plotW, plotY + plotH);
            spectrumPath.closeSubPath();

            // Gradient fill matching Equinox teal/cyan aesthetic
            juce::ColourGradient specGrad (juce::Colour (0x3500d4d8), plotX + plotW * 0.5f, plotY + plotH * 0.3f,
                                           juce::Colour (0x02000000), plotX + plotW * 0.5f, plotY + plotH, false);
            g.setGradientFill (specGrad);
            g.fillPath (spectrumPath);

            // Glowing crest line
            g.setColour (juce::Colour (0x6600f0ff));
            g.strokePath (spectrumPath, juce::PathStrokeType (1.2f));
        }

        // 5. Calculate Full Parametric Reverb Response Curve (HighPass + Mid Bell + HighCut)
        juce::Path curvePath;
        juce::Path fillPath;

        const float qHp = lowCutQ;
        const float qLp = highCutQ;
        bool firstPoint = true;

        for (float px = 0.0f; px <= plotW; px += 2.5f)
        {
            float freq = xToFreq (px, plotW);

            // 1) High-Pass magnitude (Low-Cut)
            float wHp = freq / currentLowCut;
            float magHp = (wHp * wHp) / std::sqrt ((1.0f - wHp * wHp) * (1.0f - wHp * wHp) + (wHp / qHp) * (wHp / qHp));

            // 2) Low-Pass magnitude (High-Cut)
            float wLp = freq / currentHighCut;
            float magLp = 1.0f / std::sqrt ((1.0f - wLp * wLp) * (1.0f - wLp * wLp) + (wLp / qLp) * (wLp / qLp));

            // 3) Parametric Mid Bell (Early / Tone Mid Peak)
            float bellDist = std::log2 (freq / currentMidFreq) / midQ;
            float midGainLinear = std::pow (10.0f, currentMidGainDb / 20.0f);
            float magMid = 1.0f + (midGainLinear - 1.0f) * std::exp (-0.5f * bellDist * bellDist);

            float totalMag = magHp * magLp * magMid;
            float gainDb = 20.0f * std::log10 (std::max (1.0e-5f, totalMag));
            gainDb = juce::jlimit (-36.0f, 14.0f, gainDb);

            float py = dbToY (gainDb, plotH) + plotY;

            if (firstPoint)
            {
                curvePath.startNewSubPath (plotX + px, py);
                fillPath.startNewSubPath (plotX + px, plotY + plotH);
                fillPath.lineTo (plotX + px, py);
                firstPoint = false;
            }
            else
            {
                curvePath.lineTo (plotX + px, py);
                fillPath.lineTo (plotX + px, py);
            }
        }

        fillPath.lineTo (plotX + plotW, plotY + plotH);
        fillPath.closeSubPath();

        // 6. Fill and Glow Filter Curve (Platinum to Cyan Gradient)
        juce::ColourGradient curveFillGrad (juce::Colour (0x2400f0ff), plotX, plotY,
                                            juce::Colour (0x028b5cf6), plotX + plotW, plotY + plotH, false);
        g.setGradientFill (curveFillGrad);
        g.fillPath (fillPath);

        // Soft Outer Glow Stroke
        g.setColour (juce::Colour (0x3300f0ff));
        g.strokePath (curvePath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Sharp Neon Core Stroke
        g.setColour (juce::Colour (0xffe2e8f0));
        g.strokePath (curvePath, juce::PathStrokeType (1.8f));

        // 7. Interactive Node Handles with Guideline Drops ('I', 'E', 'R')
        float nodeIX = freqToX (currentLowCut, plotW) + plotX;
        float nodeIY = dbToY (-3.0f, plotH) + plotY;

        float nodeEX = freqToX (currentMidFreq, plotW) + plotX;
        float nodeEY = dbToY (currentMidGainDb, plotH) + plotY;

        float nodeRX = freqToX (currentHighCut, plotW) + plotX;
        float nodeRY = dbToY (-3.0f, plotH) + plotY;

        // Vertical Guide Drops to Bottom Axis
        drawGuideDrop (g, nodeIX, nodeIY, plotY + plotH, juce::Colour (0x6600f0ff));
        drawGuideDrop (g, nodeEX, nodeEY, plotY + plotH, juce::Colour (0x66a855f7));
        drawGuideDrop (g, nodeRX, nodeRY, plotY + plotH, juce::Colour (0x6638bdf8));

        // Draw Interactive Node Badges matching Equinox
        drawNodeBadge (g, nodeIX, nodeIY, "I", juce::Colour (0xff00f0ff), activeDragHandle == Handle::LowCut);
        drawNodeBadge (g, nodeEX, nodeEY, "E", juce::Colour (0xffc084fc), activeDragHandle == Handle::MidEq);
        drawNodeBadge (g, nodeRX, nodeRY, "R", juce::Colour (0xff38bdf8), activeDragHandle == Handle::HighCut);

        // 8. Acoustic Analysis Laser Scan Beam
        if (isAnalyzing)
        {
            float scanX = plotX + (plotW * analyzeProgress);
            juce::ColourGradient beamGrad (juce::Colour (0x0000f0ff), scanX, plotY,
                                           juce::Colour (0xffffffff), scanX, plotY + plotH * 0.5f, true);
            g.setGradientFill (beamGrad);
            g.fillRect (scanX - 1.0f, plotY, 2.0f, plotH);

            g.setColour (juce::Colour (0xffffffff));
            g.fillEllipse (scanX - 3.0f, plotY + plotH * 0.5f - 3.0f, 6.0f, 6.0f);
        }

        // 9. Top Readout & Status Header Bar
        drawTopHeader (g, bounds);

        // 10. Bottom Filter Shape Badges (/ \ ^ V ~)
        drawBottomFilterBadges (g, bounds);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto plotArea = getPlotArea();
        float nodeIX = freqToX (currentLowCut, plotArea.getWidth()) + plotArea.getX();
        float nodeIY = dbToY (-3.0f, plotArea.getHeight()) + plotArea.getY();

        float nodeEX = freqToX (currentMidFreq, plotArea.getWidth()) + plotArea.getX();
        float nodeEY = dbToY (currentMidGainDb, plotArea.getHeight()) + plotArea.getY();

        float nodeRX = freqToX (currentHighCut, plotArea.getWidth()) + plotArea.getX();
        float nodeRY = dbToY (-3.0f, plotArea.getHeight()) + plotArea.getY();

        float distI = std::hypot (e.position.x - nodeIX, e.position.y - nodeIY);
        float distE = std::hypot (e.position.x - nodeEX, e.position.y - nodeEY);
        float distR = std::hypot (e.position.x - nodeRX, e.position.y - nodeRY);

        const float threshold = 18.0f;
        isDraggingNode = false;
        dragStartMousePos = e.position;
        dragStartLowCut = currentLowCut;
        dragStartHighCut = currentHighCut;
        dragStartMidFreq = currentMidFreq;
        dragStartMidGainDb = currentMidGainDb;

        if (distI <= threshold)
        {
            activeDragHandle = Handle::LowCut;
            selectedNode = NodeSelection::Input;
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            repaint();
        }
        else if (distE <= threshold)
        {
            activeDragHandle = Handle::MidEq;
            selectedNode = NodeSelection::Early;
            setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
            repaint();
        }
        else if (distR <= threshold)
        {
            activeDragHandle = Handle::HighCut;
            selectedNode = NodeSelection::Reverb;
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            repaint();
        }
        else
        {
            // Clicked outside node handles: NEVER move or jump any node!
            activeDragHandle = Handle::None;
            setMouseCursor (juce::MouseCursor::NormalCursor);

            // Allow selecting nodes via bottom shape icons (/  \  ^)
            auto bounds = getLocalBounds().toFloat();
            float bottomY = bounds.getBottom() - 18.0f;
            float startX = bounds.getRight() - 130.0f;
            for (int i = 0; i < 3; ++i)
            {
                auto iconRect = juce::Rectangle<float> (startX + i * 22.0f, bottomY, 18.0f, 14.0f);
                if (iconRect.contains (e.position))
                {
                    if (i == 0) selectedNode = NodeSelection::Input;
                    else if (i == 1) selectedNode = NodeSelection::Reverb;
                    else if (i == 2) selectedNode = NodeSelection::Early;
                    repaint();
                    break;
                }
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (activeDragHandle == Handle::None)
            return;

        // Require intentional hold and drag (minimum 3 pixels movement)
        // Mistaken clicks or mouse tap without holding and moving will NOT change anything
        if (! isDraggingNode)
        {
            if (e.getDistanceFromDragStart() < 3)
                return;

            isDraggingNode = true;
        }

        auto plot = getPlotArea();
        float deltaX = e.position.x - dragStartMousePos.x;

        if (activeDragHandle == Handle::LowCut)
        {
            float startX = freqToX (dragStartLowCut, plot.getWidth());
            float targetX = startX + deltaX;
            float newFreq = xToFreq (targetX, plot.getWidth());
            newFreq = juce::jlimit (20.0f, juce::jmin (2000.0f, currentHighCut - 100.0f), newFreq);
            currentLowCut = newFreq;
            if (onLowCutChanged)
                onLowCutChanged (newFreq);
            repaint();
        }
        else if (activeDragHandle == Handle::MidEq)
        {
            float startX = freqToX (dragStartMidFreq, plot.getWidth());
            float targetX = startX + deltaX;
            float newFreq = xToFreq (targetX, plot.getWidth());
            newFreq = juce::jlimit (100.0f, 8000.0f, newFreq);

            float deltaY = e.position.y - dragStartMousePos.y;
            float startY = dbToY (dragStartMidGainDb, plot.getHeight());
            float targetY = startY + deltaY;
            float newGain = yToDb (targetY, plot.getHeight());
            newGain = juce::jlimit (-12.0f, 12.0f, newGain);

            currentMidFreq = newFreq;
            currentMidGainDb = newGain;
            if (onMidEqChanged)
                onMidEqChanged (newFreq, newGain);
            repaint();
        }
        else if (activeDragHandle == Handle::HighCut)
        {
            float startX = freqToX (dragStartHighCut, plot.getWidth());
            float targetX = startX + deltaX;
            float newFreq = xToFreq (targetX, plot.getWidth());
            newFreq = juce::jlimit (juce::jmax (500.0f, currentLowCut + 100.0f), 20000.0f, newFreq);
            currentHighCut = newFreq;
            if (onHighCutChanged)
                onHighCutChanged (newFreq);
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        activeDragHandle = Handle::None;
        isDraggingNode = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        // Adjust Q value on wheel scroll
        if (selectedNode == NodeSelection::Early)
        {
            midQ = juce::jlimit (0.2f, 5.0f, midQ + wheel.deltaY * 0.5f);
            repaint();
        }
        else if (selectedNode == NodeSelection::Input)
        {
            lowCutQ = juce::jlimit (0.5f, 2.5f, lowCutQ + wheel.deltaY * 0.3f);
            repaint();
        }
        else if (selectedNode == NodeSelection::Reverb)
        {
            highCutQ = juce::jlimit (0.5f, 2.5f, highCutQ + wheel.deltaY * 0.3f);
            repaint();
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto plotArea = getPlotArea();
        float nodeIX = freqToX (currentLowCut, plotArea.getWidth()) + plotArea.getX();
        float nodeIY = dbToY (-3.0f, plotArea.getHeight()) + plotArea.getY();

        float nodeEX = freqToX (currentMidFreq, plotArea.getWidth()) + plotArea.getX();
        float nodeEY = dbToY (currentMidGainDb, plotArea.getHeight()) + plotArea.getY();

        float nodeRX = freqToX (currentHighCut, plotArea.getWidth()) + plotArea.getX();
        float nodeRY = dbToY (-3.0f, plotArea.getHeight()) + plotArea.getY();

        float distI = std::hypot (e.position.x - nodeIX, e.position.y - nodeIY);
        float distE = std::hypot (e.position.x - nodeEX, e.position.y - nodeEY);
        float distR = std::hypot (e.position.x - nodeRX, e.position.y - nodeRY);

        const float threshold = 18.0f;
        if (distI <= threshold || distR <= threshold)
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        else if (distE <= threshold)
            setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

private:
    enum class Handle { None, LowCut, MidEq, HighCut };
    enum class NodeSelection { Input, Early, Reverb };

    Handle activeDragHandle { Handle::None };
    NodeSelection selectedNode { NodeSelection::Reverb };
    bool isDraggingNode { false };
    juce::Point<float> dragStartMousePos;
    float dragStartLowCut { 80.0f };
    float dragStartHighCut { 14000.0f };
    float dragStartMidFreq { 1000.0f };
    float dragStartMidGainDb { 0.0f };

    float currentLowCut { 80.0f };
    float lowCutQ { 0.7071f };

    float currentMidFreq { 1000.0f };
    float currentMidGainDb { 0.0f };
    float midQ { 1.0f };

    float currentHighCut { 14000.0f };
    float highCutQ { 0.7071f };

    float rmsLevel { 0.0f };
    float spectrumPhase { 0.0f };
    bool isAnalyzing { false };
    float analyzeProgress { 0.0f };

    juce::Rectangle<float> getPlotArea() const
    {
        auto bounds = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
        bounds.removeFromTop (22.0f);   // Top status bar
        bounds.removeFromBottom (22.0f); // Bottom filter badges & freq scale
        return bounds;
    }

    float freqToX (float freq, float width) const
    {
        float minF = 20.0f;
        float maxF = 20000.0f;
        float norm = std::log10 (juce::jlimit (minF, maxF, freq) / minF) / std::log10 (maxF / minF);
        return norm * width;
    }

    float xToFreq (float x, float width) const
    {
        float minF = 20.0f;
        float maxF = 20000.0f;
        float norm = juce::jlimit (0.0f, 1.0f, x / width);
        return minF * std::pow (maxF / minF, norm);
    }

    float dbToY (float db, float height) const
    {
        float minDb = -36.0f;
        float maxDb = 14.0f;
        float norm = (juce::jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb);
        return (1.0f - norm) * height;
    }

    float yToDb (float y, float height) const
    {
        float minDb = -36.0f;
        float maxDb = 14.0f;
        float norm = 1.0f - juce::jlimit (0.0f, 1.0f, y / height);
        return minDb + norm * (maxDb - minDb);
    }

    void updateLowCutFromX (float mouseX)
    {
        auto plot = getPlotArea();
        float newFreq = xToFreq (mouseX - plot.getX(), plot.getWidth());
        newFreq = juce::jlimit (20.0f, juce::jmin (2000.0f, currentHighCut - 100.0f), newFreq);
        currentLowCut = newFreq;
        if (onLowCutChanged)
            onLowCutChanged (newFreq);
        repaint();
    }

    void updateMidEqFromPos (float mouseX, float mouseY)
    {
        auto plot = getPlotArea();
        float newFreq = xToFreq (mouseX - plot.getX(), plot.getWidth());
        newFreq = juce::jlimit (100.0f, 8000.0f, newFreq);

        float newGain = yToDb (mouseY - plot.getY(), plot.getHeight());
        newGain = juce::jlimit (-12.0f, 12.0f, newGain);

        currentMidFreq = newFreq;
        currentMidGainDb = newGain;

        if (onMidEqChanged)
            onMidEqChanged (newFreq, newGain);
        repaint();
    }

    void updateHighCutFromX (float mouseX)
    {
        auto plot = getPlotArea();
        float newFreq = xToFreq (mouseX - plot.getX(), plot.getWidth());
        newFreq = juce::jlimit (juce::jmax (500.0f, currentLowCut + 100.0f), 20000.0f, newFreq);
        currentHighCut = newFreq;
        if (onHighCutChanged)
            onHighCutChanged (newFreq);
        repaint();
    }

    void drawGuideDrop (juce::Graphics& g, float x, float yStart, float yEnd, juce::Colour col)
    {
        g.setColour (col);
        juce::Path p, dashed;
        p.startNewSubPath (x, yStart);
        p.lineTo (x, yEnd);
        const float dashes[] = { 3.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, p, dashes, 2);
        g.strokePath (dashed, juce::PathStrokeType (1.0f));
    }

    void drawNodeBadge (juce::Graphics& g, float x, float y, const juce::String& letter, juce::Colour col, bool isActive)
    {
        float radius = isActive ? 9.5f : 8.0f;

        // Glowing outer halo
        g.setColour (col.withAlpha (isActive ? 0.45f : 0.25f));
        g.fillEllipse (x - radius * 1.5f, y - radius * 1.5f, radius * 3.0f, radius * 3.0f);

        // Core Badge Fill (#1c1d22)
        g.setColour (juce::Colour (0xff18191f));
        g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);

        // Border ring
        g.setColour (col);
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, isActive ? 2.0f : 1.5f);

        // Centered Letter ('I', 'E', 'R')
        g.setFont (juce::FontOptions (isActive ? 10.5f : 9.5f, juce::Font::bold));
        g.setColour (col);
        g.drawText (letter, (int) (x - radius), (int) (y - radius), (int) (radius * 2.0f), (int) (radius * 2.0f), juce::Justification::centred);
    }

    void drawTopHeader (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        // Glowing cyan micro-pip & modern header
        float pipX = bounds.getX() + 12.0f;
        float pipY = bounds.getY() + 11.5f;

        g.setColour (juce::Colour (0x4400f0ff));
        g.fillEllipse (pipX - 2.5f, pipY - 2.5f, 7.0f, 7.0f);
        g.setColour (juce::Colour (0xff00f0ff));
        g.fillEllipse (pipX - 1.0f, pipY - 1.0f, 4.0f, 4.0f);

        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xffcbd5e1));
        g.drawText ("REVERB FILTER // ACOUSTIC SPECTRUM", (int) bounds.getX() + 22, (int) bounds.getY() + 4, 240, 15, juce::Justification::centredLeft);

        // Active Node Detailed Readout
        float activeFreq = (selectedNode == NodeSelection::Input) ? currentLowCut
                         : (selectedNode == NodeSelection::Early) ? currentMidFreq : currentHighCut;
        float activeQ = (selectedNode == NodeSelection::Input) ? lowCutQ
                      : (selectedNode == NodeSelection::Early) ? midQ : highCutQ;
        float activeGain = (selectedNode == NodeSelection::Early) ? currentMidGainDb : -3.0f;
        const char* nodeName = (selectedNode == NodeSelection::Input) ? "NODE [I] INPUT"
                             : (selectedNode == NodeSelection::Early) ? "NODE [E] EARLY" : "NODE [R] REVERB";

        juce::String paramText = juce::String (nodeName)
                               + "   |   FREQ: " + formatFreq (activeFreq)
                               + "   |   Q: " + juce::String (activeQ, 2)
                               + "   |   GAIN: " + (activeGain >= 0.0f ? "+" : "") + juce::String (activeGain, 1) + " dB";

        g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
        g.setColour (juce::Colour (0xff8a90a2));
        g.drawText (paramText, (int) bounds.getRight() - 360, (int) bounds.getY() + 4, 350, 15, juce::Justification::right);
    }

    void drawBottomFilterBadges (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        float bottomY = bounds.getBottom() - 18.0f;

        // Shape Icons (/  \  ^  V  ~) matching reference Equinox toolbar
        const struct ShapeIcon { const char* glyph; bool active; } shapes[] = {
            { "/",  selectedNode == NodeSelection::Input },
            { "\\", selectedNode == NodeSelection::Reverb },
            { "^",  selectedNode == NodeSelection::Early },
            { "v",  false },
            { "~",  false }
        };

        float startX = bounds.getRight() - 130.0f;
        for (int i = 0; i < 5; ++i)
        {
            auto iconRect = juce::Rectangle<float> (startX + i * 22.0f, bottomY, 18.0f, 14.0f);
            if (shapes[i].active)
            {
                g.setColour (juce::Colour (0xff27272a));
                g.fillRoundedRectangle (iconRect, 2.0f);
                g.setColour (juce::Colour (0xff00f0ff));
                g.drawRoundedRectangle (iconRect, 2.0f, 1.0f);
                g.setColour (juce::Colour (0xffffffff));
            }
            else
            {
                g.setColour (juce::Colour (0xff1f2024));
                g.fillRoundedRectangle (iconRect, 2.0f);
                g.setColour (juce::Colour (0xff71717a));
            }

            g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
            g.drawText (shapes[i].glyph, iconRect.toNearestInt(), juce::Justification::centred);
        }

        // Left Cutoff Summary Text
        g.setFont (juce::FontOptions (8.5f, juce::Font::plain));
        g.setColour (juce::Colour (0xff71717a));
        juce::String cutoffSummary = "L-CUT: " + formatFreq (currentLowCut) + "   H-CUT: " + formatFreq (currentHighCut);
        g.drawText (cutoffSummary, (int) bounds.getX() + 10, (int) bottomY, 200, 14, juce::Justification::left);
    }

    juce::String formatFreq (float hz) const
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, (hz >= 10000.0f ? 0 : 1)) + " kHz";
        return juce::String ((int) hz) + " Hz";
    }
};
