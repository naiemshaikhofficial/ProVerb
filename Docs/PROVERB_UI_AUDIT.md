# ProVerb UI Audit & Redesign Architecture Plan
**Plugin:** ProVerb (Producer Toy)  
**Task:** Full Visual / UI Redesign inspired by `assets/reference/ProVerb_UI_Reference.png`  
**Scope Boundary:** UI layer exclusively (no DSP or serialization changes)

---

## 1. Current Architecture & Components Audit

| Component | Current File | Responsibility | Redesign Implementation |
| :--- | :--- | :--- | :--- |
| **PluginEditor** | `Source/PluginEditor.h`, `Source/PluginEditor.cpp` | Root editor window, APVTS bindings, layout orchestration, header and card panel arrangement | Widescreen dashboard (1540x900 default, resizable), 5 distinct columns + header + bottom EQ panel; vector typographic "ProVerb" wordmark with "by PRODUCERTOY"; elevated graphite/navy `#0D1521` to `#101B29` panels |
| **ProVerbTokens** | `Source/UI/ProVerbTokens.h` | Central design token system | Strict colors (`#070B12`, `#0D1521`, `#243246`, `#24D7FF`, `#357DFF`, `#9460FF`, `#F34DB7`, `#FFB451`, `#35E6B5`), radii, metrics, and typography tokens |
| **ProVerbLookAndFeel** | `Source/UI/ProVerbLookAndFeel.h` | Custom drawing for rotary sliders, vertical faders, toggle buttons, combo boxes, push buttons | Precision dark graphite rotors, multi-pass luminous neon arcs, radiant indicator pips with jewel centers, capsule fader caps with illuminated slits and glowing beams, capsule buttons |
| **SpaceVisualizer** | `Source/UI/SpaceVisualizer.h` | Center acoustic space hero panel with triangular aura, 3D perspective, barycentric puck, space slider, phase correlation meter | 3D/2D View Mode perspective toggle; layered translucent organic aura; concentric resonance wavefront contours; orbital satellite beacons; BRIGHT/WARM/SPACIOUS vertex nodes; full-width rainbow Space slider; real-time stereo phase meter |
| **ModernReverbEQ** | `Source/UI/ModernReverbEQ.h` | Bottom full-width EQ & acoustic spectrum analyzer | 5-node rainbow curve (Magenta, Cyan, Mint, Amber, Coral); interactive draggable handles; frequency/dB grid; real-time FFT reverberant spectrum fill; top parameter readout bar |
| **LicensePanel & AssistantPanel** | `Source/UI/LicensePanel.*`, `Source/UI/ReverbAssistantPanel.*` | Modal overlays for activation and smart assistant | Preserved 100% functionality with consistent aesthetic styling |

---

## 2. Parameter Bindings & Telemetry Feeds

Every interactive control maps directly to existing APVTS parameters:

### Top Header:
- **Preset Selector**: `presetBox`, `prevPresetBtn`, `nextPresetBtn`, `heartPresetBtn` (bound to processor programs + user presets)
- **A / B Comparison**: `abButton` (toggles snapshots A and B)
- **Bypass**: `bypassButton` (bound to `bypass`)
- **Settings & Menu**: `settingsButton`, `menuButton`

### Module A: ROUTING & FILTERS
- **Mid/Side Toggle**: `midSideButton` -> `midSide`
- **High Cut Knob**: `hCutSlider` -> `highCut` (1000 - 20000 Hz) — Cyan `#24D7FF`
- **Low Cut Knob**: `lCutSlider` -> `lowCut` (20 - 1000 Hz) — Violet `#9460FF`
- **Duck Knob**: `duckSlider` -> `ducking` (0 - 100 %) — Amber `#FFB451`
- **Sidechain Mode**: `sidechainModeButton` -> `duckingSource`

### Module B: SPACE & TIME
- **Pre-Delay Knob**: `delaySlider` -> `predelayMs` (0 - 300 ms) — Magenta `#F34DB7`
- **Size Knob**: `sizeSlider` -> `size` (10 - 100 %) — Electric Blue `#357DFF`
- **Diffusion Knob**: `diffSlider` -> `diffusion` (0 - 100 %) — Cyan `#24D7FF`
- **Shimmer Knob**: `shimmerSlider` -> `shimmer` (0 - 100 %) — Amber `#FFB451`

### Module C: ACOUSTIC SPACE (Hero Centerpiece)
- **Interactive Puck**: Barycentric coordinates dynamically driving High Cut, Damping, Low Cut, Size, and Diffusion
- **3D / 2D View Switch**: Internal perspective projection toggle (`is3DMode`)
- **Space Slider**: Horizontal spectrum slider bound to `size` parameter
- **Phase Meter**: Displays real `audioProcessor.getStereoPhaseCorrelation()` (-1.0 to +1.0)

### Module D: DECAY & DAMP
- **Decay Time Knob**: `decSlider` -> `decaySeconds` (0.1 - 30.0 s) — Electric Blue `#357DFF`
- **Damping Knob**: `dampSlider` -> `damping` (0 - 100 %) — Magenta `#F34DB7`
- **Stereo Width Knob**: `widthSlider` -> `width` (0 - 200 %) — Amber `#FFB451`
- **Freeze Mode**: `freezeButton` -> `freeze`

### Module E: CONSOLE MIXER
- **Input Fader**: `inGainFader` -> `inputGain` (-24 to +12 dB) — Cyan `#24D7FF`
- **Dry Fader**: `dryFader` -> `dry` (0 - 100 %) — Magenta `#F34DB7`
- **Wet Fader**: `wetFader` -> `wet` (0 - 100 %) — Amber `#FFB451`
- **Output Fader**: `outGainFader` -> `outputGain` (-24 to +12 dB) — Mint Green `#35E6B5`
- **SEP Knob**: `sepSlider` -> `width` (Stereo Separation) — Violet `#9460FF`

### Module F: REVERB EQ / ACOUSTIC SPECTRUM
- **Low Cut Node (Node 1)**: Draggable handle bound to `lowCut`
- **High Cut Node (Node 5)**: Draggable handle bound to `highCut`
- **Mid Nodes (Nodes 2, 3, 4)**: Interactive tone shaping handles
- **Real-Time FFT Analyzer**: Driven by `audioProcessor.getRmsOutput()` and reverberant spectral energy

---

## 3. Brand & Visual Assets
- **Producer Toy Logo**: High-resolution official logo from `Assets/branding/ProducerToy_Logo.png` with exact aspect ratio preserved.
- **ProVerb Wordmark**: Stylized typographic wordmark "ProVerb" with bold white "Pro" and luminous blue/violet gradient "Verb", accompanied by "by PRODUCERTOY".

---

## 4. Verification & Safety Constraints
- DSP processing, parameter layouts, state serialization, presets, and audio thread behavior are 100% untouched.
- All JUCE components compile with MSVC / C++17 with 0 errors.
- Thread-safe parameter reads and atomic telemetry updates.
