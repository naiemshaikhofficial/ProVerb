# ProVerb — Second-Pass Verification & Rigorous Engineering Audit

**Project:** ProVerb — Smart AI Algorithmic Reverb & Space Engine  
**Brand:** ProducerToy  
**Target:** Windows VST3 64-bit  
**Framework:** JUCE 8.0.4, C++17, MSVC x64 (Build Tools v143)  
**Date of Verification:** October 26, 2026  
**Auditor:** Principal Audio DSP Engineer & Audio Systems Architect  
**Audit Status:** Second-Pass Verification Completed (Evidence-Based, Read-Only)

---

## Executive Summary of Second-Pass Audit

This second-pass verification was conducted to rigorously re-examine all claims made in earlier reports, reconcile numerical discrepancies, and eliminate any assumptions or marketing language that does not match the actual C++ source code in `Source/`.

### Key Verification Breakthroughs
1. **Room Size vs. RT60 Discrepancy Resolved:**  
   The previously reported RT60 values of $1.41\text{ s}$ (Size 0%), $1.87\text{ s}$ (Size 50%), and $2.00\text{ s}$ (Size 100%) were caused by **measurement harness bias in `test_dsp_suite.cpp`**, which used a crude peak-to-threshold search ($10^{-4}$ of the peak in the first 10 blocks). Because small rooms pack early energy into a sharp initial spike, the $-40\text{ dB}$ threshold was artificially elevated. When re-measured using the **ISO 3382 Schroeder backward integrated Energy Decay Curve (EDC)**, the true decay time $T_{30} \times 2$ is **$1.772\text{ s}$ at Size 0% and $1.870\text{ s}$ at Size 100%** (a tight variance of $< 5.5\%$, primarily influenced by loop soft-saturation).
2. **First-Reflection Arrival Separated from Energy Centroid:**  
   The early reflection network is an **all-pass diffusion cascade**, not a set of discrete geometric tapped delay lines. The first all-pass delay tap arrives at **$1.08\text{ ms}$ (Size 0%) to $5.74\text{ ms}$ (Size 100%)**, while the first FDN late recirculating line arrives at **$6.90\text{ ms}$ to $45.59\text{ ms}$**. The $55.3\text{ ms} \to 145.5\text{ ms}$ figures represent the **energy centroid** ($T_c = \frac{\sum t \cdot e(t)}{\sum e(t)}$) of the first $267\text{ ms}$, not the arrival time of the first reflection.
3. **Mono Compatibility Clarified:**  
   Low stereo cross-correlation ($r = 0.0606$) proves spatial decorrelation, but **does not guarantee mono energy preservation**. In ProVerb, the Width knob operates exclusively on the wet Side channel ($M \pm w \cdot S$). When folded down to mono in a DAW ($M = (L+R)/2$), the wet Side energy cancels algebraically, causing a **$-2.47\text{ dB}$ drop in wet level at Width 100% and a $-6.09\text{ dB}$ drop at Width 200%**. The mono-folded wet signal is identical across all width settings, but sounds significantly drier relative to the dry track when folded to mono.
4. **All 8 Algorithm Modes Audited:**  
   The feedback matrix is **identical across all 8 modes** (the standard $8\times 8$ orthogonal Householder matrix $A = I - 0.25 \cdot \mathbf{1}\mathbf{1}^T$). The modes do not change matrix topology; they scale delay lengths ($0.82\times \to 1.25\times$), alter diffusion limits ($0.65 \to 0.84$), adjust soft-saturation amounts ($0.18 \to 0.35$), and introduce four specific conditional branches (Wide Hall crossfeed, Plate high-decay base, Vintage 10.5 kHz low-pass filter, and Gated Schmitt envelope). Modes 0 (Neutral Room) and 4 (Smooth Chamber) are perceptually very similar (only $8\%$ delay difference).
5. **Vintage Digital Myth Dispelled:**  
   There is **no 12-bit wordlength truncation, no bit-depth quantization, and no brickwall filter** in Mode 6. The implementation consists entirely of single-pole $6\text{ dB/octave}$ low-pass feedback filtering ($f_c = 10,500\text{ Hz}$) and rational soft saturation ($y = x / (1 + 0.35 x^2)$).
6. **Real-Time Safety & Click-Free Claims Qualified:**  
   The audio callback is free of heap allocations and mutexes. Parameter sweeps on Size and gains are smooth. However, **switching Algorithm Modes and rapid parametric EQ automation are not guaranteed click-free**, because mode topology branches and biquad filter coefficients update on block boundaries without crossfading.
7. **Installer Duplicate Plugin Issue Confirmed:**  
   `Installer\Windows\ProVerb_Windows_Installer.iss` lines 44 and 47 install the VST3 bundle to both `{commoncf64}\VST3\ProVerb.vst3` and `{commoncf64}\VST3\Producer Toy\ProVerb.vst3`, causing duplicate entries in DAW plugin managers.

---

## Detailed Audit Findings by Area

### 1. Room Size and RT60 Discrepancy Resolution
- **Classification:** **Verified with an identified limitation (Harness Bias)**
- **Source Code Traced:**
  - [FDNReverb.h: Lines 226–237](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/DSP/FDNReverb.h#L226-L237) (Size scaling calculation)
  - [FDNReverb.h: Lines 266–272](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/DSP/FDNReverb.h#L266-L272) (Jot loop gain calculation)
  - [test_dsp_suite.cpp: Lines 930–972](file:///d:/Website/ProducerToy-Plugins/ProVerb/Tools/test_dsp_suite.cpp#L930-L972) (Crude T40 test harness)
  - [measure_rt60.cpp: Lines 23–108](file:///d:/Website/ProducerToy-Plugins/ProVerb/Tools/measure_rt60.cpp#L23-L108) (ISO 3382 Schroeder harness)

#### Mathematical & Empirical Analysis
The feedback loop gain for line $i$ is calculated using Jot's classic formula:
$$g_i = 10^{-3 \cdot \frac{\tau_i}{T_{60}}}$$
where $\tau_i$ is the actual delay length in seconds at the active Room Size scale factor:
$$\tau_i = \frac{\text{baseDelayLengths}[i] \cdot \text{smoothedSizeScale}}{f_s}$$
Because $g_i$ is exponentially matched to $\tau_i$, the theoretical energy decay rate of the delay tank is independent of delay length:
$$\frac{dE}{dt} = -\frac{60\text{ dB}}{T_{60}}$$

#### Measured Empirical Data (Target RT60 = 2.00 s, $f_s = 48\text{ kHz}$, Damping = 0%)
A dedicated verification harness was executed on the production `FDNReverb` engine:

| Room Size (%) | First FDN Delay ($\tau_0$) | Schroeder $T_{20} \times 3$ | Schroeder $T_{30} \times 2$ | Crude $T_{40} \times 1.5$ (`test_dsp_suite`) | Observation / Diagnosis |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **0%** | $6.90\text{ ms}$ | **$1.703\text{ s}$** | **$1.772\text{ s}$** | $1.440\text{ s}$ (Reported 1.41 s) | **Peak skew in test harness** |
| **25%** | $14.75\text{ ms}$ | **$1.834\text{ s}$** | **$1.864\text{ s}$** | $1.728\text{ s}$ | Accurate decay tracking |
| **50%** | $24.33\text{ ms}$ | **$1.832\text{ s}$** | **$1.863\text{ s}$** | $1.808\text{ s}$ (Reported 1.87 s) | Accurate decay tracking |
| **75%** | $34.69\text{ ms}$ | **$1.829\text{ s}$** | **$1.849\text{ s}$** | $1.856\text{ s}$ | Accurate decay tracking |
| **100%** | $45.59\text{ ms}$ | **$1.849\text{ s}$** | **$1.870\text{ s}$** | $1.904\text{ s}$ (Reported 2.00 s) | Accurate decay tracking |

#### Resolution of Contradiction
1. **Primary Cause (Option B):** The crude test harness in `test_dsp_suite.cpp` evaluated the peak energy in the first 10 blocks ($106\text{ ms}$) and looked for when block energy fell below $-40\text{ dB}$ relative to that peak. At Size 0%, short delay recirculations concentrate in the first $30\text{ ms}$, creating an artificially tall peak that shifts the $-40\text{ dB}$ cutoff threshold earlier, yielding $1.41\text{ s}$.
2. **Secondary Cause:** The slight deviation from 2.00 s to 1.77–1.87 s across all sizes is caused by the nonlinear soft-saturator in the loop ($y = x / (1 + 0.22 x^2)$), which provides subtle damping during high-amplitude initial bursts.
3. **Conclusion:** **Room Size does NOT destroy RT60.** True Schroeder decay time is stable within $5.5\%$ across the entire $0\% \to 100\%$ Size range.

---

### 2. First-Reflection Timing vs. Energy Centroid
- **Classification:** **Verified with an identified limitation (Clarification Required)**
- **Source Code Traced:**
  - [FDNReverb.h: Lines 42–62](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/DSP/FDNReverb.h#L42-L62) (Diffuser delay base constants)
  - [FDNReverb.h: Lines 361–390](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/DSP/FDNReverb.h#L361-L390) (All-pass early reflection cascade)
  - [FDNReverb.h: Lines 648–658](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/DSP/FDNReverb.h#L648-L658) (`processAllpass` structure)

#### Architecture Analysis
The early reflection section in ProVerb does not use discrete geometric tapped delay lines (e.g., ray tracing or image-source taps). It uses a **4-stage series all-pass filter cascade** with cross-coupling:
$$y[n] = b[n - D] + g \cdot (x[n] - g \cdot b[n - D])$$
Because an all-pass filter exhibits non-zero feedthrough at $n = 0$ ($y[0] = g \cdot x[0]$), the impulse begins passing energy immediately. The delayed energy clusters emerge at discrete tap intervals $D_0, D_1, D_2, D_3$.

#### Measured Temporal Breakdown ($f_s = 48\text{ kHz}$)

| Room Size (%) | First All-Pass Delay Tap | First FDN Tank Delay | Measured Early Centroid ($T_c$) | Early Energy / Total Energy |
| :--- | :--- | :--- | :--- | :--- |
| **0%** | $1.08\text{ ms}$ | $6.90\text{ ms}$ | **$45.10\text{ ms}$** (Test Suite: $55.3\text{ ms}$) | $96.1\%$ |
| **25%** | $2.10\text{ ms}$ | $14.75\text{ ms}$ | **$82.88\text{ ms}$** (Test Suite: $95.3\text{ ms}$) | $90.2\%$ |
| **50%** | $3.26\text{ ms}$ | $24.33\text{ ms}$ | **$102.3\text{ ms}$** (Test Suite: $119.6\text{ ms}$) | $86.1\%$ |
| **75%** | $4.48\text{ ms}$ | $34.69\text{ ms}$ | **$115.2\text{ ms}$** (Test Suite: $134.7\text{ ms}$) | $84.2\%$ |
| **100%** | $5.74\text{ ms}$ | $45.59\text{ ms}$ | **$121.9\text{ ms}$** (Test Suite: $145.5\text{ ms}$) | $81.5\%$ |

#### Key Clarification
- The previously quoted $55.3\text{ ms} \to 145.5\text{ ms}$ figures represent the **energy centroid** (center of gravity of the early impulse response over $267\text{ ms}$).
- The **first reflection tap arrival** occurs much earlier: **$1.08\text{ ms} \to 5.74\text{ ms}$**.
- The **first late-reverb FDN recirculation** arrives at **$6.90\text{ ms} \to 45.59\text{ ms}$**.

---

### 3. Stereo-to-Mono Fold-Down Compatibility
- **Classification:** **Verified with an identified limitation (Physical Law of Stereo Decorrelation)**
- **Source Code Traced:**
  - [PluginProcessor.cpp: Lines 1028–1036](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/PluginProcessor.cpp#L1028-L1036) (Width M/S stage)
  - [PluginProcessor.cpp: Lines 1047–1050](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/PluginProcessor.cpp#L1047-L1050) (Dry/wet mixing)

#### Signal Mathematics
The stereo width processing is implemented via Mid/Side manipulation:
$$\text{mid}[n] = \frac{\text{wetL}[n] + \text{wetR}[n]}{2}, \quad \text{side}[n] = \frac{\text{wetR}[n] - \text{wetL}[n]}{2} \cdot w$$
$$\text{wetL}_{\text{out}}[n] = \text{mid}[n] - \text{side}[n], \quad \text{wetR}_{\text{out}}[n] = \text{mid}[n] + \text{side}[n]$$
When summed to mono in a DAW ($\text{mono} = \frac{\text{wetL}_{\text{out}} + \text{wetR}_{\text{out}}}{2}$):
$$\text{mono}_{\text{wet}}[n] = \frac{(\text{mid}[n] - \text{side}[n]) + (\text{mid}[n] + \text{side}[n])}{2} = \text{mid}[n]$$
Because the side term cancels out algebraically:
1. **Mono wet energy is completely invariant with respect to the Width control ($w$).**
2. In stereo, increasing Width boosts side energy, increasing total stereo energy ($E_{\text{stereo}} = 2 \text{mid}^2 + 2 w^2 \text{side}^2$).
3. When folded to mono, all side energy disappears, resulting in an attenuation of the wet reverberant field relative to the dry track.

#### Empirical Measurements

| Width Setting | Stereo Wet Energy | Mono-Folded Energy | Level Delta vs. Stereo | Perceptual Character |
| :--- | :--- | :--- | :--- | :--- |
| **0%** | $0.9582$ | $0.4791$ | **$0.00\text{ dB}$** | Perfect mono; no energy lost |
| **50%** | $1.1420$ | $0.4791$ | **$-0.76\text{ dB}$** | Natural subtle space |
| **100%** | $1.6934$ | $0.4791$ | **$-2.47\text{ dB}$** | Standard wide stereo; normal room fold-down |
| **150%** | $2.6124$ | $0.4791$ | **$-4.36\text{ dB}$** | Hyper-wide; noticeable reverb loss in mono |
| **200%** | $3.8989$ | $0.4791$ | **$-6.09\text{ dB}$** | Extreme side boost; reverb recedes heavily in mono |

#### Dry Path Independence
The Width stage processes `wetBuffer` exclusively ([PluginProcessor.cpp: Line 1028](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/PluginProcessor.cpp#L1028)). The dry signal passes unaffected. Therefore, at high Width settings ($> 120\%$), folding to mono makes the mix sound substantially drier than in stereo.

---

### 4. Objective Audit of the Eight Algorithm Modes
- **Classification:** **Verified and reproduced (Topology constants confirmed)**
- **Source Code Traced:**
  - [FDNReverb.h: Lines 160–218](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/DSP/FDNReverb.h#L160-L218) (Algorithm mode switch)
  - [FDNReverb.h: Lines 293–296](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/DSP/FDNReverb.h#L293-L296) (Mode 6 10.5 kHz filter)
  - [FDNReverb.h: Lines 518–523](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/DSP/FDNReverb.h#L518-L523) (Mode 2 Wide Hall crossfeed)
  - [FDNReverb.h: Lines 526–546](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/DSP/FDNReverb.h#L526-L546) (Mode 7 Gated envelope)

#### Feedback Matrix Reality
**The feedback matrix does NOT change across modes.** The identical Householder matrix:
$$A_{i,j} = \delta_{i,j} - \frac{2}{N} = \delta_{i,j} - 0.25 \quad (N=8)$$
is used for all 8 modes.

#### Mode Comparison Matrix

| Mode Index & Name | Delay Scale | Diff Max | Sat Amount | Mod Mult | Measured $T_{30} \times 2$ | Cross-Corr $r$ | Unique Architectural Features |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **0: Neutral Room** | $1.00\times$ | $0.72$ | $0.22$ | $1.0\times$ | $1.840\text{ s}$ | $+0.1316$ | Balanced baseline prime distribution |
| **1: Dense Room** | $0.82\times$ | $0.80$ | $0.24$ | $1.0\times$ | $1.835\text{ s}$ | $+0.0551$ | Compact delays, denser all-pass smearing |
| **2: Wide Hall** | $1.25\times$ | $0.65$ | $0.20$ | $1.0\times$ | $1.837\text{ s}$ | **$-0.3170$** | Expanded delay lengths, negative crossfeed $(R-L)\cdot 0.25$ |
| **3: Plate Character** | $0.94\times$ | $0.84$ | $0.28$ | $1.0\times$ | $1.836\text{ s}$ | $+0.0601$ | `highDecayBase = 0.78` (brighter high sustain), dense diffusion |
| **4: Smooth Chamber** | $1.08\times$ | $0.70$ | $0.20$ | $1.0\times$ | $1.847\text{ s}$ | $+0.0494$ | Golden-ratio spacing; **sonically very close to Mode 0** |
| **5: Modulated Space** | $1.15\times$ | $0.70$ | $0.18$ | **$1.75\times$** | $1.842\text{ s}$ | $+0.0408$ | Enhanced dual LFO excursion, cosmic spatial drift |
| **6: Vintage Digital** | $1.00\times$ | $0.68$ | **$0.35$** | $1.0\times$ | **$1.689\text{ s}$** | $+0.1292$ | $10.5\text{ kHz}$ 1-pole LP filter inside loop, higher saturation |
| **7: Gated / Nonlinear** | $0.88\times$ | $0.76$ | $0.25$ | $1.0\times$ | **$0.452\text{ s}$** | $+0.0743$ | Threshold detector ($-45\text{ dBFS}$), cosine release gate cutoff |

#### Redundancy Finding
- Mode 0 (Neutral Room) and Mode 4 (Smooth Chamber) differ by only $8\%$ in delay lengths and have almost identical decay and correlation profiles. They are perceptually difficult to distinguish without manual listening comparisons.
- The truly differentiated modes with distinct sonic behavior are Mode 2 (Wide Hall), Mode 5 (Modulated Space), Mode 6 (Vintage Digital), and Mode 7 (Gated Reverb).

---

### 5. Vintage Digital Implementation Verification
- **Classification:** **Contradicted by current code (Correction required in documentation)**
- **Source Code Traced:**
  - [FDNReverb.h: Lines 293–296](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/DSP/FDNReverb.h#L293-L296)
  - [FDNReverb.h: Lines 485–490](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/DSP/FDNReverb.h#L485-L490)
  - [FDNReverb.h: Lines 496–503](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/DSP/FDNReverb.h#L496-L503)

#### Code Reality
- **No 12-bit wordlength truncation or integer quantization exists.** The signal remains 32-bit floating point throughout.
- **No brickwall filter exists.** The filter is a standard single-pole recursive low-pass filter:
  $$\beta = \exp\left(-\frac{2\pi \cdot 10500}{f_s}\right)$$
  $$y[n] = (1 - \beta) \cdot x[n] + \beta \cdot y[n - 1]$$
  This creates a gentle $6\text{ dB/octave}$ roll-off above $10.5\text{ kHz}$.
- **Saturation:** Implemented via a continuous smooth rational transfer function:
  $$y = \frac{x}{1 + 0.35 \cdot x^2}$$
- **Conclusion:** Mode 6 produces a warm, vintage-style roll-off and subtle harmonic drive, but it does **not** emulate vintage ADC/DAC bit reduction or steep anti-aliasing brickwall filters. Marketing and documentation descriptions must be updated accordingly.

---

### 6. Real-Time Safety & Code Quality Audit
- **Classification:** **Verified with identified limitations (Mode Switching & EQ Stepping)**
- **Source Code Traced:**
  - [PluginProcessor.cpp: Lines 695–706](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/PluginProcessor.cpp#L695-L706) (`processBlock` entry, denormals, channel clearing)
  - [PluginProcessor.cpp: Lines 917–934](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/PluginProcessor.cpp#L917-L934) (Scratch buffer chunking)
  - [ModernReverbEQ.h: Lines 180–225](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/UI/ModernReverbEQ.h#L180-L225) (EQ coefficient update)

#### Real-Time Verification Checklist

| Property | Status | Evidence / Code Location |
| :--- | :--- | :--- |
| **No Heap Allocation in Audio Thread** | **VERIFIED** | Buffers (`dryBuffer`, `wetBuffer`, `immediateBuffer`) pre-allocated in `prepareToPlay` ([PluginProcessor.cpp: Line 622](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/PluginProcessor.cpp#L622)). Inner arrays are fixed-size stack buffers. |
| **Lock-Free Processing** | **VERIFIED** | All parameter reads use atomic loads (`std::memory_order_relaxed`). AI audio stream pushes to `juce::AbstractFifo` ring buffer without mutex acquisition. |
| **Denormal Protection** | **VERIFIED** | `juce::ScopedNoDenormals noDenormals;` active at the top of `processBlock` ([PluginProcessor.cpp: Line 697](file:///d:/Website/ProducerToy-Plugins/ProVerb/Source/PluginProcessor.cpp#L697)). Direct zeroing of states $< 10^{-18}$ in FDN loop. |
| **Click-Free Size Sweeps** | **VERIFIED** | `smoothedSizeScale` glides with $35\text{ ms}$ exponential time constant; delay lines use 4-point Hermite cubic interpolation (`fastReadSample`). Maximum sample-to-sample difference during rapid full-range sweeps is $< 0.65$. |
| **Click-Free Mode Switching** | **IDENTIFIED LIMITATION** | When switching to Mode 2 (Wide Hall), the crossfeed $(R-L)\cdot 0.25$ turns on immediately without a crossfade. In Mode 7 (Gated Reverb), the gate cutoff applies immediately. Rapid mode automation in a DAW can produce audible clicks. |
| **Click-Free EQ Automation** | **IDENTIFIED LIMITATION** | EQ biquad coefficients update per 32-sample block without sample-level parameter smoothing. High-Q automation sweeps can cause micro-zippering. |
| **Freeze Stability** | **VERIFIED** | Freeze transition uses a $15\text{ ms}$ glide (`freezeFade`), clamps loop gains to 1.0, and disables nonlinear saturation. Level remains stable to within $< 0.1\text{ dB}$ over 3 seconds. |

---

### 7. Independent Test Suite Reproduction
- **Classification:** **Verified and reproduced**
- **Test Executables Run:**
  - `build\Release\ProVerb_DSPSuiteTest.exe`
  - `build\Release\ProVerb_FeatureParityTest.exe`
  - `build\Release\ProVerb_RT60Test.exe`
  - `build\Release\ProVerb_OctaveAudit.exe`
  - `build\Release\ProVerb_LicensingTest.exe`

#### Execution Summary Matrix

| Test Executable | Source Location | Total Checks | Passed | Failed | Exit Code | Verified Property |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `ProVerb_DSPSuiteTest` | `Tools/test_dsp_suite.cpp` | **53** | 53 | 0 | `0` | Size scaling, pre-delay, EQ, ducking, modes, modulation, freeze |
| `ProVerb_FeatureParityTest`| `Tools/cpp_feature_extractor_test.cpp` | **178** | 178 | 0 | `0` | C++ feature extractor, 5-band crossover, ONNX AI inference |
| `ProVerb_RT60Test` | `Tools/measure_rt60.cpp` | **12** | 12 | 0 | `0` | Schroeder RT60 decay across 44.1k/48k/96k, Freeze sustain |
| `ProVerb_OctaveAudit` | `Tools/measure_octave_rt60.cpp` | **23** | 23 | 0 | `0` | 7-band octave decay, stereo decorrelation ($r=0.0606$), SR timing |
| `ProVerb_LicensingTest` | `Tools/test_licensing.cpp` | **6** | 6 | 0 | `0` | Ed25519 device-locked tokens, tampering rejection, deactivation |
| **TOTAL ASSERTIONS** | — | **272** | **272** | **0** | — | **100% Pass Rate** |

---

### 8. Performance, CPU & Packaging Claims
- **Classification:** **Verified with identified limitations (Packaging Duplicate)**
- **Source Code Traced:**
  - [Installer/Windows/ProVerb_Windows_Installer.iss: Lines 43–48](file:///d:/Website/ProducerToy-Plugins/ProVerb/Installer/Windows/ProVerb_Windows_Installer.iss#L43-L48)
  - [CMakeLists.txt: Lines 87–120](file:///d:/Website/ProducerToy-Plugins/ProVerb/CMakeLists.txt#L87-L120)

#### High-Resolution CPU Execution Benchmark
- **Test Platform:** Windows 10/11 x64, MSVC v19.44 Release x64 optimization (`/O2 /MD`).
- **Test Signal:** Continuous complex audio, 2,000 blocks of 512 samples ($21.33\text{ seconds}$ total audio duration at $48\text{ kHz}$).
- **Measured Time:** $463.05\text{ ms}$ elapsed total.
- **Average Time per 512-Sample Block:** **$231.5\text{ microseconds}$**.
- **Real-Time Block Budget ($512 / 48000$):** $10,666.7\text{ microseconds}$.
- **Exact Single-Thread CPU Utilization:** **$2.17\%$**.
- **Explanation of Discrepancy:** Earlier reports cited $1.1\% - 1.4\%$, which represented only the isolated FDN delay loop. The measured $2.17\%$ is the true, comprehensive CPU cost of the entire plugin signal chain (FDN + Shimmer + 5-Band EQ + Vocal Ducking + Saturation + Stereo Width).

#### Installer and Packaging Inspection
- **Packaged VST3 Binary:**  
  Path: `build\ProVerb_artefacts\Release\VST3\ProVerb.vst3\Contents\x86_64-win\ProVerb.vst3`  
  Size: **$8,916,992\text{ bytes}$** ($8.50\text{ MB}$).  
  SHA-256: `06ce9229c56bbd5438549e48b76ded80b6a14167650e0feebf1d37f58bffb0ef`.  
  Matches the tested release binary.
- **Bundled Dependencies:**
  - `onnxruntime.dll`: $11,569,696\text{ bytes}$
  - `onnxruntime_providers_shared.dll`: $22,048\text{ bytes}$
  - `ProVerb_AI.onnx`: $15,848\text{ bytes}$
  - Total single installed bundle footprint: **$20,524,584\text{ bytes}$** ($20.5\text{ MB}$).
- **Installer Executable:**  
  File: `ProducerToy_ProVerb_Installer.exe`  
  Size: **$10,503,300\text{ bytes}$** ($10.02\text{ MB}$).
- **Confirmed Packaging Defect (Duplicate Plugins):**  
  In [ProVerb_Windows_Installer.iss](file:///d:/Website/ProducerToy-Plugins/ProVerb/Installer/Windows/ProVerb_Windows_Installer.iss#L43-L48), the script installs the VST3 bundle into two locations:
  1. `{commoncf64}\VST3\ProVerb.vst3`
  2. `{commoncf64}\VST3\Producer Toy\ProVerb.vst3`  
  Because DAWs recursively scan `Common Files\VST3`, DAWs such as FL Studio, Reaper, and Ableton Live will detect two identical copies of ProVerb. This consumes $41.0\text{ MB}$ of disk space instead of $20.5\text{ MB}$ and clutters plugin lists.

---

## Prioritized List of Remaining Engineering Issues

| Priority | Category | Issue Description | Impact | Recommended Solution |
| :--- | :--- | :--- | :--- | :--- |
| **P1** | **Packaging** | Installer creates duplicate VST3 copies in root and subfolder | DAWs show duplicate ProVerb entries; doubles installed disk footprint | Remove line 47 in `ProVerb_Windows_Installer.iss` so only `{commoncf64}\VST3\ProVerb.vst3` is installed |
| **P1** | **DSP Real-Time** | Algorithm Mode switching does not crossfade topology branches | Audible click/pop when switching to Wide Hall (crossfeed) or Gated mode during playback | Add a short ($15\text{–}25\text{ ms}$) equal-power crossfade between active and target mode buffers |
| **P2** | **DSP / EQ** | Parametric EQ biquad coefficients step on block boundaries | Micro-zippering artifacts during rapid DAW automation sweeps on high-Q filters | Introduce first-order exponential smoothing on filter coefficients or use TPT state-variable filters across all 5 bands |
| **P2** | **Presets** | Factory presets authored with legacy `size = 10.0` | Small room/vocal booth presets do not utilize the full $0.28\times$ closet scale at $0\%$ | Re-author small room presets in `Presets.h` with `size = 0.0` to $5.0$ |
| **P3** | **DSP Character** | Modes 0 (Neutral Room) and 4 (Smooth Chamber) lack differentiation | Redundant modes; producer cannot hear meaningful difference | Re-tune Smooth Chamber delay ratios to a true golden-ratio spacing with a distinct $1.4\times$ bass decay multiplier |
| **P3** | **Documentation** | Mode 6 described as "12-bit wordlength truncation with brickwall filter" | Inaccurate marketing claims; misleads technical users | Update preset descriptions and user manual to state: "Warm analog-style $10.5\text{ kHz}$ low-pass feedback filtering and soft harmonic saturation" |

---

## Verification Commands Executed

The following exact commands were executed to produce the empirical evidence in this report:

```powershell
# 1. Run full DSP unit test suite
.\build\Release\ProVerb_DSPSuiteTest.exe

# 2. Run C++ feature parity and ONNX inference tests
.\build\Release\ProVerb_FeatureParityTest.exe

# 3. Run ISO 3382 Schroeder RT60 and Freeze tests
.\build\Release\ProVerb_RT60Test.exe

# 4. Run 7-band octave decay and stereo decorrelation audit
.\build\Release\ProVerb_OctaveAudit.exe

# 5. Run Ed25519 licensing client production tests
.\build\Release\ProVerb_LicensingTest.exe

# 6. Compile and execute dedicated second-pass verification harness
cmd /c "C:\Users\Naiem Shaikh\.gemini\antigravity-ide\brain\8c7dd2c5-5b7d-4227-a059-337c919afa8c\scratch\compile_verifier.bat"
& "C:\Users\Naiem Shaikh\.gemini\antigravity-ide\brain\8c7dd2c5-5b7d-4227-a059-337c919afa8c\scratch\audit_verifier.exe"
```

---

## Conclusion

ProVerb contains a high-quality, mathematically sound FDN reverb core with robust RT60 scaling, smooth Hermite interpolation for the Room Size control, verified lossless Freeze capability, and efficient CPU performance ($2.17\%$).

However, **ProVerb cannot yet be considered fully production-ready for commercial release** until:
1. The duplicate VST3 installer bug is resolved.
2. Algorithm mode switching is equipped with an equal-power crossfade to eliminate clicks during live DAW switching.
3. Parametric EQ coefficients are smoothed to protect against automation zippering.
4. Promotional and documentation claims regarding "12-bit digital truncation" are corrected to accurately reflect the actual DSP implementation.

*In strict compliance with instructions, no production source files, presets, installer scripts, or Git records have been modified. Standing by for authorization before implementing any fixes.*
