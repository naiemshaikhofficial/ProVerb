#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * ProVerb Design System Tokens
 * Producer Toy | Modern Cinematic Reverb UI
 * 
 * Strict palette, typography, radii, and metrics directly derived
 * from the ProVerb reference design:
 * - Deep blue-black backgrounds
 * - Dark graphite and navy elevated panels
 * - Fine cool-gray borders with restrained inner shadows
 * - Controlled neon chromatic accents (Cyan, Electric Blue, Violet, Magenta, Amber, Mint)
 */
namespace ProVerbTokens
{
    // =========================================================================
    // 1. Color Palette (Matching ProVerb Reference)
    // =========================================================================
    namespace Colors
    {
        // Core Chassis & Backgrounds
        static const juce::Colour backgroundMain       { 0xff070b12 }; // Deep blue-black #070B12
        static const juce::Colour backgroundSecondary  { 0xff0a111b }; // Secondary background #0A111B
        static const juce::Colour panelBackground      { 0xff0d1521 }; // Main panel graphite/navy #0D1521
        static const juce::Colour panelElevated        { 0xff101b29 }; // Elevated panel #101B29
        static const juce::Colour panelDarkSlot        { 0xff080c14 }; // Dark recessed well / slot
        static const juce::Colour headerBackground     { 0xff080d16 }; // Top header bar

        // Structural Borders & Dividers
        static const juce::Colour border               { 0xff243246 }; // Fine cool-gray border #243246
        static const juce::Colour borderSubtle         { 0x333b4d66 }; // Hairline divider #3B4D66 (20% alpha)
        static const juce::Colour borderHighlight      { 0x1affffff }; // Subtle top-edge light catch (10% white)

        // Typography
        static const juce::Colour textPrimary          { 0xfff2f6fc }; // Pure clear white #F2F6FC
        static const juce::Colour textSecondary        { 0xff8b9ab0 }; // Muted blue-gray #8B9AB0
        static const juce::Colour textMuted            { 0xff65748a }; // Deep muted label #65748A
        static const juce::Colour textDim              { 0xff475569 }; // Inactive / disabled text

        // Controlled Chromatic Neons (Parameter Groups & Spectral Points)
        static const juce::Colour neonCyan             { 0xff24d7ff }; // Cyan #24D7FF (High Cut, Input, Bright)
        static const juce::Colour neonBlue             { 0xff357dff }; // Electric Blue #357DFF (Size, Decay)
        static const juce::Colour neonViolet           { 0xff9460ff }; // Violet #9460FF (Low Cut, SEP)
        static const juce::Colour neonMagenta          { 0xfff34db7 }; // Magenta #F34DB7 (Pre-Delay, Damping, Dry, Warm)
        static const juce::Colour neonAmber            { 0xffffb451 }; // Amber Gold #FFB451 (Duck, Shimmer, Wet)
        static const juce::Colour neonMint             { 0xff35e6b5 }; // Mint Green #35E6B5 (Output, Spacious, Phase)
        static const juce::Colour neonCoral            { 0xffff5a5f }; // Coral Red #FF5A5F (Bypass, High Shelf)
    }

    // =========================================================================
    // 2. Metrics & Radii
    // =========================================================================
    namespace Metrics
    {
        static constexpr float panelCornerRadius       = 14.0f;
        static constexpr float innerCardRadius         = 10.0f;
        static constexpr float buttonPillRadius        = 15.0f;
        static constexpr float faderSlotWidth          = 4.0f;
        static constexpr float faderThumbWidth         = 30.0f;
        static constexpr float faderThumbHeight        = 18.0f;
        static constexpr int   defaultWindowWidth      = 1540;
        static constexpr int   defaultWindowHeight     = 900;
        static constexpr int   minWindowWidth          = 1080;
        static constexpr int   minWindowHeight         = 640;
        static constexpr int   maxWindowWidth          = 2200;
        static constexpr int   maxWindowHeight         = 1350;
    }

    // =========================================================================
    // 3. Typography Helpers
    // =========================================================================
    namespace Typography
    {
        inline juce::Font makeTitleFont (float size)
        {
            return juce::Font (juce::FontOptions ("Segoe UI", size, juce::Font::bold));
        }

        inline juce::Font makeLabelFont (float size, bool bold = true)
        {
            return juce::Font (juce::FontOptions ("Segoe UI", size, bold ? juce::Font::bold : juce::Font::plain));
        }

        inline juce::Font makeHeaderFont (float size, bool bold = true)
        {
            return juce::Font (juce::FontOptions ("Segoe UI", size, bold ? juce::Font::bold : juce::Font::plain));
        }

        inline juce::Font makeValueFont (float size)
        {
            return juce::Font (juce::FontOptions ("Segoe UI", size, juce::Font::bold));
        }
    }
}
