# ProVerb AI Training Data — License & Compliance Audit Report
**Organization:** ProducerToy | **Plugin:** ProVerb  
**Manifest Source:** `training_data_manifest.json`  
**Total Records Audited:** 6157  

---

## 1. Executive Summary & Commercial Eligibility

| Permission Status | Count | Percentage | Commercial Suitability |
| :--- | :--- | :--- | :--- |
| `ATTRIBUTION_REQUIRED` | 6046 | 98.2% | Eligible (Attribution verified) |
| `COMMERCIALLY_PERMITTED` | 111 | 1.8% | Eligible (Attribution verified) |

---

## 2. Ingested Datasets & Provenance

| Dataset Name | Stems / Recordings | Ingested License | Verified Source URL |
| :--- | :--- | :--- | :--- |
| **Slakh2100** | 209 | `CC-BY-4.0` | [https://www.slakh.com/](https://www.slakh.com/) |
| **VocalSet** | 3613 | `CC-BY-4.0` | [https://doi.org/10.5281/zenodo.1193957](https://doi.org/10.5281/zenodo.1193957) |
| **GuitarSet** | 360 | `CC-BY-4.0` | [https://doi.org/10.5281/zenodo.3371780](https://doi.org/10.5281/zenodo.3371780) |
| **WaivOps-SYN-SE1** | 1000 | `CC-BY-4.0` | [https://doi.org/10.5281/zenodo.13993751](https://doi.org/10.5281/zenodo.13993751) |
| **OpenMIC-2018** | 975 | `CC-BY-4.0` | [https://doi.org/10.5281/zenodo.1432913](https://doi.org/10.5281/zenodo.1432913) |

---

## 3. Acoustic Class Representation Audit

| Class Index | ProVerb Class Name | Recorded Examples | Data Provenance & Realism Status |
| :---: | :--- | :--- | :--- |
| [0] | **Drums** | 165 | 🟢 Present in manifest |
| [1] | **Vocals** | 3689 | 🟢 Present in manifest |
| [2] | **Guitar** | 621 | 🟢 Present in manifest |
| [3] | **Piano / Keys** | 206 | 🟢 Present in manifest |
| [4] | **Bass** | 48 | 🟢 Present in manifest |
| [5] | **Synth / Pad** | 1092 | 🟢 Present in manifest |
| [6] | **Other / Uncertain** | 336 | 🟢 Present in manifest |

---

## 4. Legal Attributions & Required Notices

- **Slakh2100**: *Slakh2100 is licensed under CC BY 4.0 by Ethan Manilow, Gordon Wichern, Prem Seetharaman, Jonathan Le Roux (Mitsubishi Electric Research Laboratories).*
- **VocalSet**: *VocalSet is licensed under CC BY 4.0 by Julia Wilkins, Prem Seetharaman, Alison Wahl, Bryan Pardo (Northwestern University).*
- **OpenMIC-2018**: *OpenMIC-2018 is licensed under CC BY 4.0 by Eric J. Humphrey, Simon Durand, and Brian McFee (Spotify & NYU MARL).*
- **GuitarSet**: *GuitarSet: Qingyang Xi, Rachel M. Bittner, Johan Pauwels, Xuzhou Ye, and Juan P. Bello, "GuitarSet: A Dataset for Guitar Transcription," ISMIR 2018. Licensed under CC BY 4.0.*
- **WaivOps-SYN-SE1**: *WaivOps SYN-SE1, Patchbanks, licensed under CC BY 4.0; https://doi.org/10.5281/zenodo.13993751.*

---

## 5. Critical Compliance Findings

- **Real Vocals Gap:** Slakh2100 is an algorithmic MIDI-synthesized dataset and does not contain human vocal tracks. Models trained exclusively on Slakh cannot claim certified real human vocal classification.
- **MUSDB18 Policy:** MUSDB18 is restricted to academic non-commercial research and is excluded from the default commercial build.
- **Attribution Requirement:** CC BY 4.0 datasets require attribution in commercial plugin credits and documentation.

---

## 6. Quarantined & Excluded Records Audit Trail

**Audit Action:** `REMOVED_FROM_ACTIVE_TRAINING_MANIFEST`  
**Note:** Replaced initial mock references with verified real audio on disk; quarantined unverified template entries.

| Recording ID | Previous Path | Dataset | Reason for Exclusion / Quarantine |
| :--- | :--- | :--- | :--- |
| `Slakh_Track00001_S01` | `Data\Slakh\Track00001\stems\S01_drums.wav` | Slakh2100 | Initial template reference without physical audio on disk. Replaced by verified BabySlakh stems. |
| `Slakh_Track00001_S02` | `Data\Slakh\Track00001\stems\S02_bass.wav` | Slakh2100 | Initial template reference without physical audio on disk. Replaced by verified BabySlakh stems. |
| `Slakh_Track00001_S03` | `Data\Slakh\Track00001\stems\S03_guitar.wav` | Slakh2100 | Initial template reference without physical audio on disk. Replaced by verified BabySlakh stems. |
| `Slakh_Track00002_S01` | `Data\Slakh\Track00002\stems\S01_piano.wav` | Slakh2100 | Initial template reference without physical audio on disk. Replaced by verified BabySlakh stems. |
| `Slakh_Track00002_S02` | `Data\Slakh\Track00002\stems\S02_pad.wav` | Slakh2100 | Initial template reference without physical audio on disk. Replaced by verified BabySlakh stems. |
| `FS_51234` | `Data\Freesound\vocals\fs_51234_female_vocal_phrase.wav` | Freesound | Fictional template reference without verified audio on disk. Quarantined from real training pipeline. |
| `FS_67890` | `Data\Freesound\guitar\fs_67890_clean_fingerpicking.wav` | Freesound | Fictional template reference without verified audio on disk. Quarantined from real training pipeline. |
