# How to Train & Update ProVerb AI Model
**ProducerToy — ProVerb Smart Algorithmic Reverb**  
*Step-by-Step Production Guide for Adding Audio Samples, Re-training the AI Model, and Deploying into VST3 & Standalone Plugins*

---

## 1. Overview & System Architecture

ProVerb uses an offline AI inference worker operating on a background CPU thread to classify incoming audio into one of **7 acoustic classes** and generate optimal reverb parameters via the `ReverbDecisionEngine`.

### Class Contract (1:1 with C++ `AudioFeatureExtractor.h`):
| Class Index | Class Name | Typical Instruments / Sources |
| :---: | :--- | :--- |
| **0** | **Drums** | Kicks, Snares, Claps, Hi-Hats, Cymbals, Drum Loops, Percussion, Tablas |
| **1** | **Vocals** | Lead Vocals, Backing Vocals, Chants, Chops, Acapellas, Singing |
| **2** | **Guitar** | Acoustic Steel/Nylon Guitar, Electric Guitar, Sitar, Tumbi, Mandolin |
| **3** | **Piano / Keys** | Grand Piano, Upright Piano, Electric Rhodes, Organ, Harmonium |
| **4** | **Bass** | 808 Sub-Bass, Electric Bass, Synth Basslines, Sub Drops |
| **5** | **Synth / Pad** | Atmospheric Pads, Polyphonic Synths, Leads, Flutes, Arps |
| **6** | **Other / Uncertain** | Complex Full Mixes, FX, Sweeps, Risers, Ambient Textures |

---

## 2. Directory Locations for Training Data

Aap naye samples ko `D:\Samples\Data Train\` ke andar organize karke rakh sakte hain:

```text
D:\Samples\Data Train\
├── Vocals\         <- Vocal stems, acapellas, vocal chops (.wav, .flac)
├── Drums\          <- Drum loops, kicks, snares, percussion
├── Bass\           <- 808s, sub-bass, basslines
├── Guitar\         <- Acoustic & electric guitar stems
├── Piano\          <- Keys, grand piano, Rhodes
└── Synth\          <- Pads, leads, synth loops
```

> **Note:** Supported file formats are `.wav`, `.flac`, `.aiff`, `.aif`, and `.mp3`. Files can be mono or stereo at 44.1 kHz, 48 kHz, or 96 kHz (downmixing and sample-rate scaling is handled automatically).

---

## 3. Step-by-Step Training Commands

Terminal ya PowerShell me plugin folder par navigate karein:
```powershell
cd "d:\Website\ProducerToy-Plugins\ProVerb"
```

### Step 1: Manifest Update (Ingest New Samples)
Jab bhi aap `D:\Samples\Data Train\` me naye audio files dalein, pehle manifest update karein:
```powershell
python Tools/build_augmented_manifest.py
```
**Yeh script kya karegi:**
- `D:\Samples\` ke sabhi naye audio files scan karegi.
- Filename aur folder keywords ke mutabiq unhe sahi class (0 to 6) me categorize karegi.
- File integrity ke liye SHA-256 hash aur audio metadata calculate karegi.
- Updated dataset ko `Assets/training_data_manifest.json` me save karegi.

---

### Step 2: Model Train Karna

Aapke paas do training modes hain:

#### Option A: Candidate Model Train Karna (Safe Testing Mode)
Agar aap pehle model test karna chahte hain bina active plugin ko modify kiye:
```powershell
python Tools/train_proverb_model.py --manifest Assets/training_data_manifest.json --candidate-only --class-weighting sqrt
```
- Model save hoga: `Assets/ProVerb_AI_candidate.onnx`.
- Active plugin model (`Assets/ProVerb_AI.onnx`) bilkul **untouched** rahega.
- Terminal me per-class precision, recall, F1-score aur confusion matrix display hoga.

#### Option B: Active Plugin me Train & Deploy Karna (Production Mode)
Jab aap model ko active plugin me apply karna chahein:
```powershell
python Tools/train_proverb_model.py --manifest Assets/training_data_manifest.json --promote-to-active --class-weighting sqrt
```
**Yeh command automatically sab kuch handle karti hai:**
1. GroupKFold (zero data leakage) ke sath deep neural network train karti hai.
2. In-graph feature normalization bake karke ONNX export karti hai (`Assets/ProVerb_AI.onnx`).
3. Purane model ka automatic backup banati hai (`ProVerb_AI.onnx.bak`).
4. Naye model ko sabhi search locations me deploy karti hai:
   - `build/Release/ProVerb_AI.onnx`
   - `build/ProVerb_artefacts/Release/Standalone/ProVerb_AI.onnx`
   - `build/ProVerb_artefacts/Release/VST3/ProVerb.vst3/Contents/x86_64-win/ProVerb_AI.onnx`
   - `C:\Program Files\Common Files\VST3\ProVerb.vst3\Contents\x86_64-win\ProVerb_AI.onnx`
5. Strict C++ harness (`ProVerb_FeatureParityTest.exe --strict`) run karke 100% numerical parity verify karti hai.

---

### Step 3: Kisi Bhi Audio File Ko Test / Predict Karna

Kisi bhi specific audio stem ka classification check karne ke liye:

**Active Model se check karna:**
```powershell
python Tools/train_proverb_ai.py --predict "D:\Samples\Data Train\Vocals\065_Bb_I_Thought_I_Knew_Su_Vox.wav"
```

**Candidate Model se check karna:**
```powershell
python Tools/train_proverb_ai.py --predict "D:\Samples\Data Train\Vocals\065_Bb_I_Thought_I_Knew_Su_Vox.wav" --onnx-path "Assets/ProVerb_AI_candidate.onnx"
```

Output me aapko 9-D acoustic features aur sabhi 7 classes ki percentage probabilities dikhayega:
```text
=================================================================
  PROVERB AI ACOUSTIC INSPECTION: 065_Bb_I_Thought_I_Knew_Su_Vox.wav
=================================================================

--- 9-Dimensional Acoustic Feature Extraction ---
  • subRatio          :   0.0430 ratio
  • bassRatio         :   0.1371 ratio
  • vocalCoreRatio    :   0.6305 ratio
  • vocalPresRatio    :   0.1684 ratio
  • airRatio          :   0.0210 ratio
  • crestFactor       :   7.2477
  • zcr               :   0.1471
  • transientDensity  :   1.6667 hits/s
  • envDynamics       :   2.6471

--- Model Class Probabilities ---
  Vocals             :  ...%
  ...
>>> PREDICTED CATEGORY : VOCALS
=================================================================
```

---

## 4. C++ Plugin Rebuild & Strict Verification

Jab bhi aap C++ source files (jaise `ReverbDecisionEngine.h`) modify karein, plugin ko compile aur verify karne ke commands:

```powershell
# 1. Test harness build karein
cmake --build build --config Release --target ProVerb_FeatureParityTest

# 2. Strict C++ Test suite run karein (178/178 tests pass hone chahiye)
.\build\Release\ProVerb_FeatureParityTest.exe --strict

# 3. Full Plugin (VST3 + Standalone) build karein
cmake --build build --config Release
```

---

## 5. Performance & Caching

- `Tools/train_proverb_model.py` me **Disk Feature Caching** (`Assets/manifest_features_cache.npz`) enabled hai.
- Pehli baar me sabhi 9,000+ files ke features extract hone ke baad cache me store ho jate hain.
- Subsequent training runs poore dataset ko re-extract karne ke bajaye **1 second** me cache se load kar lete hain.
- Agar aap `Assets/training_data_manifest.json` me naye files add karenge, cache automatically detect karke update ho jata hai.
