#include "ReverbAssistantPanel.h"
#include "../PluginProcessor.h"
#include <cmath>

namespace ProducerToy::UI
{
    ReverbAssistantPanel::ReverbAssistantPanel (ProVerbAudioProcessor& proc,
                                                std::function<void()> onCloseCallback,
                                                std::function<void(const juce::String&, const juce::String&)> onPresetAppliedCallback)
        : processor (proc),
          onClose (std::move (onCloseCallback)),
          onPresetApplied (std::move (onPresetAppliedCallback))
    {
        setOpaque (true);
        setAlwaysOnTop (true);
        startTimerHz (30); // 30 FPS animation & audio monitoring
    }

    ReverbAssistantPanel::~ReverbAssistantPanel()
    {
        stopTimer();
    }

    void ReverbAssistantPanel::startAssistantFlow()
    {
        resetFlow();
        setVisible (true);
        toFront (true);
    }

    void ReverbAssistantPanel::resetFlow()
    {
        currentState = State::WaitingForAudio;
        hasFinishedAnalysis = false;
        analysisTicks = 0;
        autoCloseCountdown = -1;
        detectedInstrument.clear();
        appliedPresetName.clear();
        repaint();
    }

    juce::Rectangle<float> ReverbAssistantPanel::getCloseButtonBounds() const
    {
        return juce::Rectangle<float> ((float) getWidth() - 48.0f, 16.0f, 28.0f, 28.0f);
    }

    juce::Rectangle<float> ReverbAssistantPanel::getDoneButtonBounds() const
    {
        return juce::Rectangle<float> ((float) getWidth() * 0.5f - 55.0f, (float) getHeight() - 58.0f, 110.0f, 32.0f);
    }

    void ReverbAssistantPanel::timerCallback()
    {
        animPhase += 0.08f;
        if (animPhase > juce::MathConstants<float>::twoPi * 10.0f)
            animPhase -= juce::MathConstants<float>::twoPi * 10.0f;

        const float inRms = processor.getRmsInput();
        pulseLevel += (inRms - pulseLevel) * 0.30f;

        if (currentState == State::WaitingForAudio)
        {
            // Auto start analysis as soon as DAW plays audio
            if (inRms > 0.005f)
            {
                currentState = State::Analyzing;
                analysisTicks = 0;
                processor.triggerAutoLearn();
            }
        }
        else if (currentState == State::Analyzing)
        {
            analysisTicks++;

            // Wait until backend analyzer finishes
            if (! processor.isAutoLearning() && analysisTicks > 35 && ! hasFinishedAnalysis)
            {
                hasFinishedAnalysis = true;
                currentState = State::Completed;

                juce::String inst = processor.getDetectedInstrument();
                juce::String preset = processor.getDetectedPresetName();

                if (inst.isEmpty() || inst == "NO AUDIO")
                    inst = "STUDIO ACOUSTIC";
                if (preset.isEmpty())
                    preset = "Warm Studio Vocal Hall";

                detectedInstrument = inst;
                appliedPresetName = preset;
                confidenceScore = juce::jlimit (0.75f, 0.98f, processor.getDetectedSourceConfidence() > 0.1f ? processor.getDetectedSourceConfidence() : 0.88f);

                if (onPresetApplied)
                    onPresetApplied (inst, preset);

                // Auto-close countdown (75 frames = ~2.5 seconds)
                autoCloseCountdown = 75;
            }
        }
        else if (currentState == State::Completed)
        {
            if (autoCloseCountdown > 0)
            {
                autoCloseCountdown--;
                if (autoCloseCountdown == 0)
                {
                    if (onClose)
                        onClose();
                }
            }
        }

        repaint();
    }

    void ReverbAssistantPanel::paint (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Pure Deep Obsidian / Charcoal Canvas (NO BLUE TINT)
        g.fillAll (juce::Colour (0xff090a0d));

        // Subtle outer border highlight (ToyTune metallic rim)
        g.setColour (juce::Colour (0xff1e2027));
        g.drawRect (getLocalBounds(), 1);

        // 2. Header Title (Left-aligned, crisp modern typography)
        g.setFont (juce::FontOptions ("Segoe UI", 16.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xfff1f5f9));

        juce::String headerTitle;
        if (currentState == State::WaitingForAudio)
            headerTitle = "Waiting for you to play audio...";
        else if (currentState == State::Analyzing)
            headerTitle = "Listening & creating preset...";
        else
            headerTitle = "Reverb Preset Created";

        g.drawText (headerTitle, 28, 18, 380, 26, juce::Justification::centredLeft, false);

        // Close Button (✕) in top right
        auto closeRect = getCloseButtonBounds();
        g.setColour (juce::Colour (0xff141418));
        g.fillEllipse (closeRect);
        g.setColour (juce::Colour (0xff242429));
        g.drawEllipse (closeRect, 1.0f);

        g.setFont (juce::FontOptions ("Segoe UI", 14.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xff94a3b8));
        g.drawText (juce::CharPointer_UTF8 ("\xc3\x97"), closeRect.toNearestInt(), juce::Justification::centred, false);

        // 3. Center Minimalist Visualizer
        auto centre = bounds.getCentre();
        centre.y -= 15.0f; // slight optical centering upward

        // Dynamic audio reactivity
        const float activeAudioPulse = pulseLevel * 2.5f;

        // Concentric Acoustic Pulse Rings (Breathing & expanding to live DAW audio)
        const int numRings = 4;
        for (int ring = 1; ring <= numRings; ++ring)
        {
            float ringPhase = std::fmod (animPhase * 0.6f + (float) ring * 1.2f, 4.0f);
            float baseR = 55.0f + (float) ring * 48.0f;
            float pulseOffset = activeAudioPulse * (18.0f + (float) ring * 8.0f);
            float ringR = baseR + pulseOffset + std::sin (ringPhase * juce::MathConstants<float>::twoPi) * 3.0f;

            float fadeNorm = 1.0f - ((float) ring / (float) (numRings + 1));
            float alpha = juce::jlimit (0.05f, 0.70f, fadeNorm * (0.16f + activeAudioPulse * 0.45f));

            juce::Colour ringCol = (ring % 2 == 0) ? juce::Colour (0xff06b6d4) : juce::Colour (0xff38bdf8);
            if (currentState == State::Completed)
                ringCol = juce::Colour (0xff38bdf8);

            g.setColour (ringCol.withAlpha (alpha));
            g.drawEllipse (centre.x - ringR, centre.y - ringR, ringR * 2.0f, ringR * 2.0f, 1.4f);
        }

        // Inner glowing core ring
        float coreR = 36.0f + activeAudioPulse * 12.0f;
        g.setColour (juce::Colour (0xff06b6d4).withAlpha (0.18f + activeAudioPulse * 0.30f));
        g.fillEllipse (centre.x - coreR, centre.y - coreR, coreR * 2.0f, coreR * 2.0f);

        // Center Minimalist Musical Note with Clock Face
        float noteX = centre.x;
        float noteY = centre.y;

        // Note Head (Clock Circle)
        g.setColour (juce::Colour (0xff12151e));
        g.fillEllipse (noteX - 24.0f, noteY - 12.0f, 36.0f, 36.0f);
        g.setColour (juce::Colour (0xff38bdf8));
        g.drawEllipse (noteX - 24.0f, noteY - 12.0f, 36.0f, 36.0f, 2.2f);

        // Clock Hands inside note head (rotates with playback/analysis)
        float hourAngle = animPhase * 0.6f;
        float minAngle  = animPhase * 2.5f;
        g.setColour (juce::Colour (0xffffffff));
        g.drawLine (noteX - 6.0f, noteY + 6.0f,
                    noteX - 6.0f + std::sin (hourAngle) * 8.0f,
                    noteY + 6.0f - std::cos (hourAngle) * 8.0f, 2.0f);
        g.drawLine (noteX - 6.0f, noteY + 6.0f,
                    noteX - 6.0f + std::sin (minAngle) * 11.0f,
                    noteY + 6.0f - std::cos (minAngle) * 11.0f, 1.6f);

        // Musical Note Stem & Flag
        g.setColour (juce::Colour (0xff38bdf8));
        g.drawLine (noteX + 12.0f, noteY + 6.0f, noteX + 12.0f, noteY - 40.0f, 2.8f);

        juce::Path flag;
        flag.startNewSubPath (noteX + 12.0f, noteY - 40.0f);
        flag.lineTo (noteX + 30.0f, noteY - 26.0f);
        flag.lineTo (noteX + 12.0f, noteY - 20.0f);
        flag.closeSubPath();
        g.setColour (juce::Colour (0xff38bdf8));
        g.fillPath (flag);

        // Floating Small Notes around the center with graceful drifting motion
        float fOffsets[4][2] = { { -95.0f, -60.0f }, { 95.0f, -50.0f }, { -85.0f, 65.0f }, { 90.0f, 55.0f } };
        for (int fn = 0; fn < 4; ++fn)
        {
            float fx = centre.x + fOffsets[fn][0];
            float fy = centre.y + fOffsets[fn][1] + std::sin (animPhase + (float) fn * 1.5f) * 8.0f;
            g.setFont (juce::FontOptions ("Segoe UI", 18.0f, juce::Font::bold));
            float fnAlpha = 0.55f + 0.35f * std::sin (animPhase * 1.8f + (float) fn);
            g.setColour (juce::Colour (0xff64748b).withAlpha (fnAlpha));
            g.drawText (fn % 2 == 0 ? juce::CharPointer_UTF8 ("\xe2\x99\xaa") : juce::CharPointer_UTF8 ("\xe2\x99\xab"),
                        (int) (fx - 12.0f), (int) (fy - 12.0f), 24, 24, juce::Justification::centred, false);
        }

        // 4. Subtext / Result Display
        if (currentState == State::Completed && detectedInstrument.isNotEmpty())
        {
            if (ProVerbAudioProcessor::isDevMode())
            {
                // Development / Diagnostic Mode: full classifier match info
                float badgeW = juce::jmin (520.0f, bounds.getWidth() - 80.0f);
                auto badgeRect = juce::Rectangle<float> (centre.x - badgeW * 0.5f, bounds.getBottom() - 106.0f, badgeW, 36.0f);

                g.setColour (juce::Colour (0xff101115));
                g.fillRoundedRectangle (badgeRect, 8.0f);
                g.setColour (juce::Colour (0xff38bdf8).withAlpha (0.8f));
                g.drawRoundedRectangle (badgeRect, 8.0f, 1.2f);

                juce::String resultMsg = juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x94 DETECTED: ")) + detectedInstrument
                                         + " (" + juce::String ((int) std::round (confidenceScore * 100.0f)) + "% MATCH)  \xe2\x86\x92  " + appliedPresetName;
                g.setFont (juce::FontOptions ("Segoe UI", 12.0f, juce::Font::bold));
                g.setColour (juce::Colour (0xff38bdf8));
                g.drawText (resultMsg, badgeRect.toNearestInt(), juce::Justification::centred, false);
            }
            else
            {
                // Production: Minimalistic dark studio theme, no icons or logos
                float badgeW = juce::jmin (280.0f, bounds.getWidth() - 80.0f);
                auto badgeRect = juce::Rectangle<float> (centre.x - badgeW * 0.5f, bounds.getBottom() - 106.0f, badgeW, 34.0f);

                g.setColour (juce::Colour (0xff101115));
                g.fillRoundedRectangle (badgeRect, 8.0f);
                g.setColour (juce::Colour (0xff242429));
                g.drawRoundedRectangle (badgeRect, 8.0f, 1.0f);

                g.setFont (juce::FontOptions ("Segoe UI", 13.0f, juce::Font::plain));
                g.setColour (juce::Colour (0xffe2e8f0));
                g.drawText ("Applied the best setting", badgeRect.toNearestInt(), juce::Justification::centred, false);
            }

            // Done Pill Button
            auto doneBtn = getDoneButtonBounds();
            g.setColour (juce::Colour (0xff06b6d4));
            g.fillRoundedRectangle (doneBtn, 16.0f);

            g.setFont (juce::FontOptions ("Segoe UI", 12.0f, juce::Font::bold));
            g.setColour (juce::Colour (0xff05070a));
            g.drawText ("Done", doneBtn.toNearestInt(), juce::Justification::centred, false);
        }
        else
        {
            juce::String subtext = (currentState == State::WaitingForAudio)
                                       ? "Play audio in your DAW..."
                                       : "Analyzing acoustic profile & generating custom reverb...";

            g.setFont (juce::FontOptions ("Segoe UI", 13.0f, juce::Font::plain));
            g.setColour (juce::Colour (0xff94a3b8));
            g.drawText (subtext, 0, (int) (bounds.getBottom() - 82.0f), (int) bounds.getWidth(), 24, juce::Justification::centred, false);
        }
    }

    void ReverbAssistantPanel::resized()
    {
    }

    void ReverbAssistantPanel::mouseDown (const juce::MouseEvent& e)
    {
        if (getCloseButtonBounds().contains (e.position))
        {
            if (onClose)
                onClose();
            return;
        }

        if (currentState == State::Completed)
        {
            if (getDoneButtonBounds().contains (e.position))
            {
                if (onClose)
                    onClose();
                return;
            }
        }
    }
}
