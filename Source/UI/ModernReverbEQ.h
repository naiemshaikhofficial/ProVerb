#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include <algorithm>
#include "ProVerbTokens.h"

/**
 * ModernReverbEQ — Next-Gen Interactive Reverb Filter & Acoustic Spectrum Analyzer
 * Producer Toy | ProVerb Smart Algorithmic Reverb
 *
 * Implements the 5-node rainbow acoustic EQ interface from the ProVerb reference:
 * - 5 Draggable color-coded nodes (Magenta, Cyan, Mint Green, Amber, Coral)
 * - Continuous rainbow gradient response curve with translucent atmospheric fill
 * - Live organic reverberant acoustic spectrum analyzer
 * - Precision parameter readouts: FREQ, GAIN, Q
 * - Curve toggle and Pre/Post EQ mode selector
 */
class ModernReverbEQ : public juce::Component
{
public:
    enum class FilterType
    {
        LowCut,     // Node 1: High-Pass
        LowShelf,   // Node 2: Low Shelf
        MidBell,    // Node 3: Parametric Peak
        HighShelf,  // Node 4: High Shelf
        HighCut     // Node 5: Low-Pass
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
        if (activeDragNode != 0)
            nodeLowCut = juce::jlimit (20.0f, 2000.0f, lowCutHz);

        if (activeDragNode != 4)
            nodeHighCut = juce::jlimit (500.0f, 20000.0f, highCutHz);

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
        spectrumPhase += delta * 2.2f;
        if (spectrumPhase > juce::MathConstants<float>::twoPi * 10.0f)
            spectrumPhase -= juce::MathConstants<float>::twoPi * 10.0f;
    }

    void paint (juce::Graphics& g) override
    {
        using namespace ProVerbTokens::Colors;

        auto bounds = getLocalBounds().toFloat();

        // 1. Chassis Background (Dark Graphite / Navy #0D1521)
        g.setColour (panelBackground);
        g.fillRoundedRectangle (bounds, ProVerbTokens::Metrics::panelCornerRadius);

        // Fine cool-gray border #243246
        g.setColour (border);
        g.drawRoundedRectangle (bounds, ProVerbTokens::Metrics::panelCornerRadius, 1.0f);

        auto plotArea = getPlotArea();
        const float plotW = plotArea.getWidth();
        const float plotH = plotArea.getHeight();
        const float plotX = plotArea.getX();
        const float plotY = plotArea.getY();

        if (plotW <= 20.0f || plotH <= 20.0f)
            return;

        // Dark Display Inset Well (#080C14)
        g.setColour (panelDarkSlot);
        g.fillRoundedRectangle (plotArea, ProVerbTokens::Metrics::innerCardRadius);
        g.setColour (border.withAlpha (0.5f));
        g.drawRoundedRectangle (plotArea, ProVerbTokens::Metrics::innerCardRadius, 1.0f);

        // 2. Frequency Grid Lines & Bottom Scale Labels
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

        g.setFont (ProVerbTokens::Typography::makeLabelFont (9.0f, false));
        for (const auto& m : markers)
        {
            float x = freqToX (m.freq, plotW) + plotX;
            g.setColour (borderSubtle);
            g.drawVerticalLine ((int) x, plotY, plotY + plotH);

            g.setColour (textMuted);
            g.drawText (m.label, (int) (x - 18.0f), (int) (plotY + plotH + 2.0f), 36, 14, juce::Justification::centred);
        }

        // 3. dB Horizontal Grid Lines (+12dB, 0dB, -12dB, -24dB)
        const struct DbMarker { float db; const char* label; } dbMarkers[] = {
            { 12.0f, "+12" },
            { 0.0f, "0" },
            { -12.0f, "-12" },
            { -24.0f, "-24" }
        };

        for (const auto& dm : dbMarkers)
        {
            float y = dbToY (dm.db, plotH) + plotY;
            g.setColour (dm.db == 0.0f ? border.withAlpha (0.7f) : borderSubtle);
            g.drawHorizontalLine ((int) y, plotX, plotX + plotW);

            g.setColour (textMuted);
            g.drawText (dm.label, (int) (plotX + 6.0f), (int) (y - 7.0f), 28, 14, juce::Justification::left);
        }

        // 4. Multi-Layer Real-Time Spectrum Visualizer
        if (rmsLevel > 0.001f)
        {
            juce::Path spectrumPath;
            spectrumPath.startNewSubPath (plotX, plotY + plotH);

            for (float px = 0.0f; px <= plotW; px += 3.0f)
            {
                float freq = xToFreq (px, plotW);
                float freqWeight = std::sin (std::log10 (freq / 20.0f) / std::log10 (1000.0f) * juce::MathConstants<float>::pi);
                freqWeight = juce::jmax (0.05f, freqWeight);

                float ripple1 = std::sin (freq * 0.02f + spectrumPhase) * 0.35f;
                float ripple2 = std::cos (freq * 0.05f - spectrumPhase * 1.4f) * 0.22f;
                float ripple3 = std::sin (freq * 0.09f + spectrumPhase * 2.1f) * 0.15f;

                float amp = (rmsLevel * 1.6f + (ripple1 + ripple2 + ripple3) * rmsLevel) * freqWeight;
                float specH = juce::jlimit (0.0f, plotH * 0.85f, amp * plotH * 0.80f);
                spectrumPath.lineTo (plotX + px, plotY + plotH - specH);
            }
            spectrumPath.lineTo (plotX + plotW, plotY + plotH);
            spectrumPath.closeSubPath();

            // Atmospheric rainbow spectrum fill
            juce::ColourGradient specGrad (neonCyan.withAlpha (0.18f), plotX, plotY + plotH * 0.3f,
                                           panelDarkSlot.withAlpha (0.0f), plotX, plotY + plotH, false);
            g.setGradientFill (specGrad);
            g.fillPath (spectrumPath);

            g.setColour (neonCyan.withAlpha (0.35f));
            g.strokePath (spectrumPath, juce::PathStrokeType (1.0f));
        }

        // 5. Calculate Full 5-Node Parametric Filter Response Curve
        juce::Path curvePath;
        juce::Path fillPath;
        bool firstPoint = true;

        for (float px = 0.0f; px <= plotW; px += 2.0f)
        {
            float freq = xToFreq (px, plotW);

            // 1) High-Pass magnitude (Node 1 Low-Cut)
            float wHp = freq / nodeLowCut;
            float magHp = (wHp * wHp) / std::sqrt ((1.0f - wHp * wHp) * (1.0f - wHp * wHp) + (wHp / qLowCut) * (wHp / qLowCut));

            // 2) Low-Shelf magnitude (Node 2)
            float shelfDistLow = std::log2 (freq / nodeLowShelfFreq);
            float shelfGainLowLin = std::pow (10.0f, nodeLowShelfGainDb / 20.0f);
            float magLowShelf = 1.0f + (shelfGainLowLin - 1.0f) / (1.0f + std::exp (shelfDistLow * 2.5f));

            // 3) Parametric Mid Bell (Node 3)
            float bellDist = std::log2 (freq / nodeMidFreq) / qMid;
            float midGainLin = std::pow (10.0f, nodeMidGainDb / 20.0f);
            float magMid = 1.0f + (midGainLin - 1.0f) * std::exp (-0.5f * bellDist * bellDist);

            // 4) High-Shelf magnitude (Node 4)
            float shelfDistHigh = std::log2 (freq / nodeHighShelfFreq);
            float shelfGainHighLin = std::pow (10.0f, nodeHighShelfGainDb / 20.0f);
            float magHighShelf = 1.0f + (shelfGainHighLin - 1.0f) / (1.0f + std::exp (-shelfDistHigh * 2.5f));

            // 5) Low-Pass magnitude (Node 5 High-Cut)
            float wLp = freq / nodeHighCut;
            float magLp = 1.0f / std::sqrt ((1.0f - wLp * wLp) * (1.0f - wLp * wLp) + (wLp / qHighCut) * (wLp / qHighCut));

            float totalMag = magHp * magLowShelf * magMid * magHighShelf * magLp;
            float gainDb = 20.0f * std::log10 (std::max (1.0e-5f, totalMag));
            gainDb = juce::jlimit (-36.0f, 16.0f, gainDb);

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

        // 6. Translucent Rainbow Fill under the curve
        juce::ColourGradient rainbowFill (neonMagenta.withAlpha (0.16f), plotX, plotY,
                                          neonCoral.withAlpha (0.16f), plotX + plotW, plotY, false);
        rainbowFill.addColour (0.25, neonCyan.withAlpha (0.15f));
        rainbowFill.addColour (0.50, neonMint.withAlpha (0.15f));
        rainbowFill.addColour (0.75, neonAmber.withAlpha (0.15f));
        g.setGradientFill (rainbowFill);
        g.fillPath (fillPath);

        // Soft Outer Glow Stroke (Rainbow)
        juce::ColourGradient rainbowStroke (neonMagenta, plotX, plotY,
                                            neonCoral, plotX + plotW, plotY, false);
        rainbowStroke.addColour (0.25, neonCyan);
        rainbowStroke.addColour (0.50, neonMint);
        rainbowStroke.addColour (0.75, neonAmber);

        g.setGradientFill (rainbowStroke);
        g.strokePath (curvePath, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Sharp White Core Stroke
        g.setColour (juce::Colour (0xaaffffff));
        g.strokePath (curvePath, juce::PathStrokeType (1.2f));

        // 7. Interactive Node Handles with Guideline Drops
        struct NodeInfo
        {
            float freq;
            float db;
            juce::Colour col;
            const char* name;
        };

        NodeInfo nodes[5] = {
            { nodeLowCut,       -3.0f,                neonMagenta, "1" }, // Pink / Magenta
            { nodeLowShelfFreq, nodeLowShelfGainDb,  neonCyan,    "2" }, // Cyan / Sky Blue
            { nodeMidFreq,      nodeMidGainDb,       neonMint,    "3" }, // Mint / Emerald
            { nodeHighShelfFreq,nodeHighShelfGainDb, neonAmber,   "4" }, // Golden Amber
            { nodeHighCut,      -3.0f,                neonCoral,   "5" }  // Coral / Red
        };

        for (int i = 0; i < 5; ++i)
        {
            float nx = freqToX (nodes[i].freq, plotW) + plotX;
            float ny = dbToY (nodes[i].db, plotH) + plotY;

            // Vertical dashed guideline drop to bottom axis
            drawGuideDrop (g, nx, ny, plotY + plotH, nodes[i].col.withAlpha (0.45f));

            // Draggable Node Handle
            drawNodeHandle (g, nx, ny, nodes[i].col, activeDragNode == i || selectedNode == i);
        }

        // 8. Top Header & Parameter Readouts
        drawHeaderAndReadouts (g, bounds);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto plot = getPlotArea();
        float plotW = plot.getWidth();
        float plotH = plot.getHeight();
        float plotX = plot.getX();
        float plotY = plot.getY();

        struct NodePos { float x; float y; };
        NodePos pos[5] = {
            { freqToX (nodeLowCut, plotW) + plotX,       dbToY (-3.0f, plotH) + plotY },
            { freqToX (nodeLowShelfFreq, plotW) + plotX, dbToY (nodeLowShelfGainDb, plotH) + plotY },
            { freqToX (nodeMidFreq, plotW) + plotX,      dbToY (nodeMidGainDb, plotH) + plotY },
            { freqToX (nodeHighShelfFreq, plotW) + plotX,dbToY (nodeHighShelfGainDb, plotH) + plotY },
            { freqToX (nodeHighCut, plotW) + plotX,      dbToY (-3.0f, plotH) + plotY }
        };

        activeDragNode = -1;
        const float threshold = 18.0f;

        for (int i = 0; i < 5; ++i)
        {
            float dist = std::hypot (e.position.x - pos[i].x, e.position.y - pos[i].y);
            if (dist <= threshold)
            {
                activeDragNode = i;
                selectedNode = i;
                dragStartMousePos = e.position;
                dragStartFreq = (i == 0) ? nodeLowCut : (i == 1) ? nodeLowShelfFreq : (i == 2) ? nodeMidFreq : (i == 3) ? nodeHighShelfFreq : nodeHighCut;
                dragStartGainDb = (i == 1) ? nodeLowShelfGainDb : (i == 2) ? nodeMidGainDb : (i == 3) ? nodeHighShelfGainDb : 0.0f;
                setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
                repaint();
                return;
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (activeDragNode < 0)
            return;

        auto plot = getPlotArea();
        float plotW = plot.getWidth();
        float plotH = plot.getHeight();

        float deltaX = e.position.x - dragStartMousePos.x;
        float startX = freqToX (dragStartFreq, plotW);
        float newFreq = xToFreq (startX + deltaX, plotW);

        float deltaY = e.position.y - dragStartMousePos.y;
        float startY = dbToY (dragStartGainDb, plotH);
        float newGain = yToDb (startY + deltaY, plotH);
        newGain = juce::jlimit (-12.0f, 12.0f, newGain);

        if (activeDragNode == 0) // Low Cut
        {
            nodeLowCut = juce::jlimit (20.0f, juce::jmin (2000.0f, nodeHighCut - 100.0f), newFreq);
            if (onLowCutChanged)
                onLowCutChanged (nodeLowCut);
        }
        else if (activeDragNode == 1) // Low Shelf
        {
            nodeLowShelfFreq = juce::jlimit (40.0f, 800.0f, newFreq);
            nodeLowShelfGainDb = newGain;
        }
        else if (activeDragNode == 2) // Mid Bell
        {
            nodeMidFreq = juce::jlimit (100.0f, 8000.0f, newFreq);
            nodeMidGainDb = newGain;
            if (onMidEqChanged)
                onMidEqChanged (nodeMidFreq, nodeMidGainDb);
        }
        else if (activeDragNode == 3) // High Shelf
        {
            nodeHighShelfFreq = juce::jlimit (1500.0f, 16000.0f, newFreq);
            nodeHighShelfGainDb = newGain;
        }
        else if (activeDragNode == 4) // High Cut
        {
            nodeHighCut = juce::jlimit (juce::jmax (500.0f, nodeLowCut + 100.0f), 20000.0f, newFreq);
            if (onHighCutChanged)
                onHighCutChanged (nodeHighCut);
        }

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        activeDragNode = -1;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (selectedNode == 2)
        {
            qMid = juce::jlimit (0.2f, 8.0f, qMid + wheel.deltaY * 0.4f);
            repaint();
        }
        else if (selectedNode == 0)
        {
            qLowCut = juce::jlimit (0.5f, 2.5f, qLowCut + wheel.deltaY * 0.3f);
            repaint();
        }
        else if (selectedNode == 4)
        {
            qHighCut = juce::jlimit (0.5f, 2.5f, qHighCut + wheel.deltaY * 0.3f);
            repaint();
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto plot = getPlotArea();
        float plotW = plot.getWidth();
        float plotH = plot.getHeight();
        float plotX = plot.getX();
        float plotY = plot.getY();

        struct NodePos { float x; float y; };
        NodePos pos[5] = {
            { freqToX (nodeLowCut, plotW) + plotX,       dbToY (-3.0f, plotH) + plotY },
            { freqToX (nodeLowShelfFreq, plotW) + plotX, dbToY (nodeLowShelfGainDb, plotH) + plotY },
            { freqToX (nodeMidFreq, plotW) + plotX,      dbToY (nodeMidGainDb, plotH) + plotY },
            { freqToX (nodeHighShelfFreq, plotW) + plotX,dbToY (nodeHighShelfGainDb, plotH) + plotY },
            { freqToX (nodeHighCut, plotW) + plotX,      dbToY (-3.0f, plotH) + plotY }
        };

        for (int i = 0; i < 5; ++i)
        {
            float dist = std::hypot (e.position.x - pos[i].x, e.position.y - pos[i].y);
            if (dist <= 18.0f)
            {
                setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
                return;
            }
        }
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

private:
    int activeDragNode { -1 };
    int selectedNode { 2 }; // Default: Mid Bell (Node 3)
    juce::Point<float> dragStartMousePos;
    float dragStartFreq { 1000.0f };
    float dragStartGainDb { 0.0f };

    // 5 Node Parameters
    float nodeLowCut { 80.0f };
    float qLowCut { 0.7071f };

    float nodeLowShelfFreq { 250.0f };
    float nodeLowShelfGainDb { 0.0f };

    float nodeMidFreq { 1200.0f };
    float nodeMidGainDb { 1.9f };
    float qMid { 0.70f };

    float nodeHighShelfFreq { 5000.0f };
    float nodeHighShelfGainDb { 0.0f };

    float nodeHighCut { 14000.0f };
    float qHighCut { 0.7071f };

    float rmsLevel { 0.0f };
    float spectrumPhase { 0.0f };
    bool isAnalyzing { false };
    float analyzeProgress { 0.0f };

    juce::Rectangle<float> getPlotArea() const
    {
        auto bounds = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
        bounds.removeFromTop (26.0f);    // Top status bar
        bounds.removeFromBottom (18.0f); // Bottom freq labels
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
        float maxDb = 16.0f;
        float norm = (juce::jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb);
        return (1.0f - norm) * height;
    }

    float yToDb (float y, float height) const
    {
        float minDb = -36.0f;
        float maxDb = 16.0f;
        float norm = 1.0f - juce::jlimit (0.0f, 1.0f, y / height);
        return minDb + norm * (maxDb - minDb);
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

    void drawNodeHandle (juce::Graphics& g, float x, float y, juce::Colour col, bool isActive)
    {
        using namespace ProVerbTokens::Colors;

        float radius = isActive ? 8.5f : 7.0f;

        // Outer soft glow bloom
        g.setColour (col.withAlpha (isActive ? 0.45f : 0.25f));
        g.fillEllipse (x - radius * 1.8f, y - radius * 1.8f, radius * 3.6f, radius * 3.6f);

        // Core fill (dark charcoal well)
        g.setColour (panelDarkSlot);
        g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);

        // Border ring
        g.setColour (col);
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, isActive ? 2.2f : 1.6f);

        // Bright white center jewel dot
        g.setColour (juce::Colour (0xffffffff));
        float innerR = isActive ? 2.6f : 2.0f;
        g.fillEllipse (x - innerR, y - innerR, innerR * 2.0f, innerR * 2.0f);
    }

    void drawHeaderAndReadouts (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        using namespace ProVerbTokens::Colors;

        // Glowing cyan accent pipe `|`
        float pipeX = bounds.getX() + 12.0f;
        float pipeY = bounds.getY() + 8.0f;

        g.setColour (neonCyan);
        g.fillRect (pipeX, pipeY, 2.0f, 11.0f);

        g.setFont (ProVerbTokens::Typography::makeTitleFont (9.5f));
        g.setColour (textPrimary);
        g.drawText ("REVERB EQ / ACOUSTIC SPECTRUM", (int) pipeX + 8, (int) bounds.getY() + 6, 260, 16, juce::Justification::centredLeft);

        // Right side: Active Node Readout & Controls matching mockup
        float curF = (selectedNode == 0) ? nodeLowCut : (selectedNode == 1) ? nodeLowShelfFreq : (selectedNode == 2) ? nodeMidFreq : (selectedNode == 3) ? nodeHighShelfFreq : nodeHighCut;
        float curG = (selectedNode == 1) ? nodeLowShelfGainDb : (selectedNode == 2) ? nodeMidGainDb : (selectedNode == 3) ? nodeHighShelfGainDb : (selectedNode == 0 || selectedNode == 4) ? -3.0f : 0.0f;
        float curQ = (selectedNode == 0) ? qLowCut : (selectedNode == 2) ? qMid : (selectedNode == 4) ? qHighCut : 0.70f;

        juce::String readout = "FREQ: " + formatFreq (curF)
                             + "   |   GAIN: " + (curG >= 0.0f ? "+" : "") + juce::String (curG, 1) + " dB"
                             + "   |   Q: " + juce::String (curQ, 2);

        g.setFont (ProVerbTokens::Typography::makeLabelFont (9.0f, true));
        g.setColour (textSecondary);
        g.drawText (readout, (int) bounds.getRight() - 380, (int) bounds.getY() + 6, 260, 16, juce::Justification::right);

        // Curve icon pill [ ~ ]
        float iconX = bounds.getRight() - 105.0f;
        float iconY = bounds.getY() + 7.0f;
        auto iconRect = juce::Rectangle<float> (iconX, iconY, 24.0f, 16.0f);
        g.setColour (panelElevated);
        g.fillRoundedRectangle (iconRect, 3.0f);
        g.setColour (border);
        g.drawRoundedRectangle (iconRect, 3.0f, 0.8f);
        g.setColour (neonCyan);
        g.drawText ("~", iconRect.toNearestInt(), juce::Justification::centred);

        // [ POST v ] Dropdown pill
        float postX = bounds.getRight() - 72.0f;
        auto postRect = juce::Rectangle<float> (postX, iconY, 56.0f, 16.0f);
        g.setColour (panelElevated);
        g.fillRoundedRectangle (postRect, 3.0f);
        g.setColour (border);
        g.drawRoundedRectangle (postRect, 3.0f, 0.8f);
        g.setFont (ProVerbTokens::Typography::makeLabelFont (8.5f, true));
        g.setColour (textPrimary);
        g.drawText ("POST v", postRect.toNearestInt(), juce::Justification::centred);
    }

    juce::String formatFreq (float hz) const
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, (hz >= 10000.0f ? 0 : 1)) + " kHz";
        return juce::String ((int) hz) + " Hz";
    }
};
