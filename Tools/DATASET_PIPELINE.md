# ProVerb AI — Real Licensed Audio Dataset & Production Training Pipeline

**ProducerToy — ProVerb Smart Algorithmic Reverb**  
*Reproducible, License-Aware Dataset Ingestion, Provenance Auditing & Model Training Guide*

---

## 1. Overview & Architecture Policy

ProVerb uses an offline AI inference worker operating purely on a background CPU thread to classify incoming audio into one of 7 acoustic classes (`Drums`, `Vocals`, `Guitar`, `Piano/Keys`, `Bass`, `Synth/Pad`, `Other/Uncertain`) and recommend algorithmic reverb parameters via the deterministic `ReverbDecisionEngine`.

### Core Engineering Invariants
1. **Zero DSP / Parameter Regressions:** The Feedback Delay Network (FDN), early reflection network, APVTS parameters, and audio-thread DSP remain 100% untouched.
2. **Offline Inference:** No AI calculations ever touch the real-time audio thread.
3. **9-Feature Contract:** 5-band complementary IIR spectral ratios, crest factor, zero-crossing rate (ZCR), transient density, and envelope dynamics.
4. **Honest Provenance & Legal Auditing:** Synthetic models are explicitly labeled `SYNTHETIC_DATA_PROTOTYPE`. Real-data models must be license-audited and tracked in `Assets/training_data_manifest.json`. Never claim production accuracy without multi-studio validation.

---

## 2. Dataset Legal Status & Acquisition Guidelines

| Dataset | Source URL | Ingested License | Commercial Status | Role in ProVerb Pipeline |
| :--- | :--- | :--- | :--- | :--- |
| **OpenMIC-2018** | [zenodo.org/records/1432913](https://zenodo.org/records/1432913) | Dataset CC BY 4.0; recording license checked per metadata | Only verified CC0/plain CC-BY recordings admitted | Weakly labeled real 10-second music clips; single-label trainer accepts only one observed canonical positive. |
| **GuitarSet** | [zenodo.org/records/3371780](https://zenodo.org/records/3371780) | **CC BY 4.0** | Permitted (Attribution Req.) | Real acoustic guitar excerpts with mono pickup mixes; grouped by player to keep players disjoint across splits. |
| **WaivOps SYN-SE1** | [zenodo.org/records/13993751](https://zenodo.org/records/13993751) | **CC BY 4.0** | Permitted (Attribution Req.) | 1,000 analog synth bass/lead one-shots with note and preset metadata; useful timbre coverage, but not pad-specific or full-mix data. |
| **VocalSet** | [zenodo.org/records/1193957](https://zenodo.org/records/1193957) | **CC BY 4.0** | Permitted (Attribution Req.) | 3,600+ genuine singing voice recordings for pure vocal acoustics. |
| **Slakh2100** | [slakh.com](https://www.slakh.com/) | **CC BY 4.0** | Permitted (Attribution Req.) | Isolated multitrack instrument stems (Drums, Bass, Guitar, Keys, Synth). |
| **BUT Speech@FIT Reverb DB** | [speech.fit.vut.cz](https://speech.fit.vut.cz/software/but-speech-fit-reverb-database) | **CC BY 4.0** | Permitted (Attribution Req.) | Room Impulse Responses (RIRs) for acoustic space augmentation. |
| **Freesound** | [freesound.org](https://freesound.org/) | **CC0 1.0 / CC BY 4.0** | Permitted (Strict filter) | Supplemental recordings (acoustic guitar, piano, vocals). |
| **MUSDB18** | [zenodo.org](https://zenodo.org/records/1117372) | **Non-Commercial Research** | **EXCLUDED** | **Excluded from commercial pipeline.** Academic research only. |

### A. Acquiring OpenMIC-2018 (Spotify & NYU MARL)
1. Official Zenodo Record: `1432913` (DOI: 10.5281/zenodo.1432913). Licensed under CC BY 4.0.
2. Run automated downloader with byte-exact verification and HTTP Range resume:
   ```bash
   python Tools/download_openmic.py
   ```
   For slow single-connection downloads, use the parallel range fetcher, then run the
   standard downloader to re-verify the archive and safely extract it:
   ```bash
   python Tools/download_openmic_ranges.py
   python Tools/download_openmic.py
   ```
3. Run the artist-disjoint importer with class balancing:
   ```bash
   python Tools/dataset_importers/openmic_importer.py --max-per-class 1200 --min-confidence 0.65
   ```

   The official archive contains `openmic-2018-aggregated-labels.csv` and
   `openmic-2018-metadata.csv`; the importer uses both and admits only recordings whose
   per-recording license is explicitly recognized as CC0 or plain CC BY. NC, ND, SA,
   Sampling+, and missing/unknown licenses are excluded from commercial training.

   OpenMIC annotations are partial, weak multi-label tags on mixed music, not isolated stems
   or proof of a single dominant source. The current seven-way trainer is single-label, so
   the importer keeps only clips with one observed high-confidence canonical positive and
   records the weak-label caveat in each manifest entry. Treat resulting models as candidates;
   do not promote without separate real-world stem/mix evaluation.

### B. Acquiring VocalSet (Northwestern University)
1. Official Zenodo Record: `1193957` (DOI: 10.5281/zenodo.1193957). Licensed under CC BY 4.0.
2. Run automated downloader:
   ```bash
   python Tools/download_vocalset.py
   ```
3. Run singer-disjoint importer:
   ```bash
   python Tools/dataset_importers/vocalset_importer.py
   ```

### C. Acquiring Slakh2100
1. Visit [https://www.slakh.com/](https://www.slakh.com/) and review terms. Slakh2100 is licensed under CC BY 4.0.
2. Download official dataset release archive and unpack into `Data/Slakh/`.

### D. Focused Guitar and Synth Coverage (GuitarSet + SYN-SE1)
1. Download the selected archives and verify their published MD5 values before extraction:
   ```bash
   python Tools/download_guitarset_synse1.py
   ```
   GuitarSet uses the official `audio_mono-pickup_mix.zip`; SYN-SE1's JSON metadata is
   downloaded alongside its WAV archive. Both are extracted under `Data/`.
2. Add/rebuild these dataset entries in the manifest (other dataset entries are preserved):
   ```bash
   python Tools/dataset_importers/instrument_focus_importer.py
   ```
   GuitarSet entries are grouped by performer, not by individual excerpt. SYN-SE1 entries
   are grouped by synth preset, not by note, to prevent near-identical notes from a preset
   leaking into both training and held-out splits. The synth data covers analog bass/lead
   tones; it is a supplemental source and does not claim dedicated pad coverage.
3. Re-run dataset integrity and license audits before training:
   ```bash
   python Tools/validate_dataset.py
   python Tools/audit_licenses.py Assets/training_data_manifest.json
   ```

### E. Acquiring BUT Speech@FIT Reverb Database
1. Visit [https://speech.fit.vut.cz/software/but-speech-fit-reverb-database](https://speech.fit.vut.cz/software/but-speech-fit-reverb-database).
2. Download the room impulse response audio files.
3. Unpack into `Data/BUT_Reverb/`.
4. Run the validator tool to check channel counts, finiteness, sample rates, and $T_{60}$ estimation:
   ```bash
   python Tools/dataset_importers/rir_importer.py Data/BUT_Reverb/
   ```

### E. Supplemental Recordings via Freesound
1. Prepare a JSON collection of audio clips acquired under verified **CC0 1.0** or **CC BY 4.0** licenses.
2. Run the license filter and auditor:
   ```bash
   python Tools/dataset_importers/freesound_importer.py UserData/freesound_manifest.json Assets/training_data_manifest.json
   ```
3. Any recording marked with non-commercial restrictions (`CC-BY-NC`), sampling-plus, or AI training opt-out is quarantined and excluded.

---

## 3. Dataset Ingestion & Manifest Generation

ProVerb tracks all source recordings, parent track IDs (for leakage-free `GroupKFold` splitting), licenses, and hashes in `Assets/training_data_manifest.json`.

### Step 1: Scan Slakh2100
```bash
python Tools/dataset_importers/slakh_importer.py Data/Slakh Assets/training_data_manifest.json
```

### Step 2: Run License & Compliance Audit
```bash
python Tools/audit_licenses.py Assets/training_data_manifest.json
```
This generates a detailed audit report: `Assets/license_audit_report.md` documenting:
- Commercial permission breakdown
- Dataset sources and attribution text
- Class representation audit (specifically highlighting unsupported classes such as Vocals)

---

## 4. Model Training & Export

The training pipeline supports two operating modes:

### Mode A: Real Labeled Audio Training (Audited Manifest)
```bash
python Tools/train_proverb_model.py --manifest Assets/training_data_manifest.json --candidate-only --class-weighting sqrt
```
- Decodes WAV/FLAC/AIFF/OGG audio files referenced in the manifest.
- Extracts features with exact C++ filter bank and dynamic feature parity.
- Splits into Train, Val, and Test using **source multitrack grouping** (ensuring all stems from the same multitrack stay in the same split).
- Normalization parameters are calculated **strictly from the training split**.
- Optional `--class-weighting sqrt` applies square-root inverse-frequency weights computed from the training split only. The report records the weights, held-out accuracy, macro-F1, and per-class precision/recall/F1.
- If unsupported classes are detected (e.g. Vocals missing from Slakh), the pipeline halts and reports the gap. To proceed with experimental training on available classes:
  ```bash
  python Tools/train_proverb_model.py --manifest Assets/training_data_manifest.json --allow-incomplete-classes
  ```
- **Candidate Staging Isolation:** By default, manifest-based training saves the model to `Assets/ProVerb_AI_candidate.onnx` and evaluates it in isolation. The active plugin model `Assets/ProVerb_AI.onnx` remains **completely untouched**.
- To promote a verified candidate model to active plugin distribution:
  ```bash
  python Tools/train_proverb_model.py --manifest Assets/training_data_manifest.json --promote-to-active
  ```

### Mode B: Synthetic Acoustic Prototype Training
```bash
python Tools/train_proverb_model.py --allow-synthetic
```
- Procedurally synthesizes acoustic examples for all 7 classes.
- Explicitly documents provenance as `SYNTHETIC_DATA_PROTOTYPE`.
- Creates backups of existing artifacts (`ProVerb_AI.onnx.bak`, `feature_norm_params.json.bak`, `model_provenance.json.bak`).
- Validates ONNX graph and ORT CPU numerical parity ($\Delta < 10^{-6}$).
- Deploys model to build targets and standalone/VST3 distribution paths.

---

## 5. BabySlakh Acquisition Tool

To inspect storage requirements and acquire the 20-track subset legally from Zenodo (Record 4603870, CC BY 4.0):
```bash
# 1. Check storage and specifications
py Tools/download_slakh_subset.py --info

# 2. Download and extract BabySlakh (841.9 MB)
py Tools/download_slakh_subset.py --download-babyslakh

# 3. Scan and generate manifest with real SHA256 hashes
py Tools/dataset_importers/slakh_importer.py Data/Slakh Assets/training_data_manifest.json
```

---

## 5. Parity & C++ Test Verification

### 1. Direct Python $\leftrightarrow$ C++ Parity Test
Verify that Python's feature extraction perfectly matches the compiled C++ `AudioAnalyzerEngine` binary:
```bash
python Tools/train_proverb_model.py --test-cpp-parity
```
*(Verified maximum absolute difference across all 9 features: $< 1 \times 10^{-6}$)*

### 2. CTest Test Suite
Run the automated test targets:
```bash
ctest --test-dir build -C Release --output-on-failure
```
- `ProVerb_FeatureParityTest_Standard`: Verifies filter banks, dynamics, NaN/Inf recovery, and 5 sample rates.
- `ProVerb_FeatureParityTest_Strict`: Enforces that `ProVerb_AI.onnx` is present, valid, and evaluates inference in strict release mode.
