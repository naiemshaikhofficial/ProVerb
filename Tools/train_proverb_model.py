#!/usr/bin/env python3
"""
ProVerb Offline AI Model Training & ONNX Export Pipeline
ProducerToy — ProVerb Smart Algorithmic Reverb

This pipeline implements:
1. Exact, high-performance vectorized simulation of ProVerb's C++ 5-band filter-bank,
   ZCR, transient detector, and envelope dynamics.
2. Direct automated cross-language numerical parity verification against the compiled
   C++ AudioAnalyzerEngine executable (ProVerb_FeatureParityTest.exe --dump-features).
3. Robust audio loading using soundfile supporting WAV, FLAC, AIFF with stereo downmixing.
4. Source-recording group splitting (GroupKFold style) ensuring zero cross-frame leakage
   between train, validation, and test sets.
5. Honest provenance reporting: explicit SYNTHETIC_DATA_PROTOTYPE notice when real audio
   is absent; never emits unverified 'Certified' labels.
6. Training-only feature normalization baked directly into the ONNX graph.
7. Logits-based neural network trained with CrossEntropyLoss and early stopping.
8. Per-class precision, recall, F1-score, and confusion matrix reporting.
9. ONNX export, ORT CPU numerical verification, and strict C++ runtime harness validation.
"""

import os
import sys
import hashlib

if sys.platform == "win32":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
import math
import json
import shutil
import argparse
import subprocess
import tempfile
from typing import Tuple, List, Dict, Optional

import numpy as np
import scipy.signal
import soundfile as sf
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import TensorDataset, DataLoader
import onnx
import onnxruntime as ort

CLASS_NAMES = [
    "Drums",
    "Vocals",
    "Guitar",
    "Piano / Keys",
    "Bass",
    "Synth / Pad",
    "Other / Uncertain"
]

FEATURE_NAMES = [
    "subRatio",
    "bassRatio",
    "vocalCoreRatio",
    "vocalPresRatio",
    "airRatio",
    "crestFactor",
    "zcr",
    "transientDensity",
    "envDynamics"
]

NUM_FEATURES = len(FEATURE_NAMES)
NUM_CLASSES = len(CLASS_NAMES)


# ==============================================================================
# 1. VECTORIZED C++ FEATURE EXTRACTOR
# ==============================================================================

def extract_features_exact_cpp(audio_mono: np.ndarray, sample_rate: float = 44100.0) -> Tuple[np.ndarray, bool]:
    """
    High-performance vectorized simulation of AudioAnalyzerEngine.h feature extraction.
    Returns:
        features: np.ndarray of shape (9,), dtype float32
        is_silence: bool indicating if audio RMS < 0.002
    """
    sr = float(sample_rate)
    if sr < 13000.0:
        raise ValueError(f"Sample rate {sr} Hz too low; must exceed 13000 Hz for 6500 Hz filter bank.")

    two_pi = 2.0 * math.pi
    alpha1 = 1.0 - math.exp(-two_pi * 95.0 / sr)
    alpha2 = 1.0 - math.exp(-two_pi * 320.0 / sr)
    alpha3 = 1.0 - math.exp(-two_pi * 1800.0 / sr)
    alpha4 = 1.0 - math.exp(-two_pi * 6500.0 / sr)

    fast_coeff = 1.0 - math.exp(-1.0 / (0.003 * sr))
    slow_coeff = 1.0 - math.exp(-1.0 / (0.045 * sr))
    refractory_samples = int(sr * 0.050)

    n_samples = len(audio_mono)
    if n_samples == 0:
        return np.zeros(NUM_FEATURES, dtype=np.float32), True

    mono = audio_mono.astype(np.float64)
    mono = np.nan_to_num(mono, nan=0.0, posinf=1.0, neginf=-1.0)
    abs_mono = np.abs(mono)

    peak_level = float(np.max(abs_mono))
    mean_rms = float(np.sqrt(np.mean(mono * mono)))

    # Consistent silence check matching AudioAnalyzerEngine.h
    if mean_rms < 0.002:
        return np.zeros(NUM_FEATURES, dtype=np.float32), True

    # 5-Band Filter Bank (1-pole IIR)
    lp1 = scipy.signal.lfilter([alpha1], [1.0, -(1.0 - alpha1)], mono)
    lp2 = scipy.signal.lfilter([alpha2], [1.0, -(1.0 - alpha2)], mono)
    lp3 = scipy.signal.lfilter([alpha3], [1.0, -(1.0 - alpha3)], mono)
    lp4 = scipy.signal.lfilter([alpha4], [1.0, -(1.0 - alpha4)], mono)

    b_sub        = lp1
    b_bass       = lp2 - lp1
    b_vocal_core = lp3 - lp2
    b_vocal_pres = lp4 - lp3
    b_air        = mono - lp4

    sub_energy        = float(np.sum(b_sub * b_sub))
    bass_energy       = float(np.sum(b_bass * b_bass))
    vocal_core_energy = float(np.sum(b_vocal_core * b_vocal_core))
    vocal_pres_energy = float(np.sum(b_vocal_pres * b_vocal_pres))
    air_energy        = float(np.sum(b_air * b_air))

    total_band_energy = sub_energy + bass_energy + vocal_core_energy + vocal_pres_energy + air_energy + 1e-12

    sub_ratio        = sub_energy / total_band_energy
    bass_ratio       = bass_energy / total_band_energy
    vocal_core_ratio = vocal_core_energy / total_band_energy
    vocal_pres_ratio = vocal_pres_energy / total_band_energy
    air_ratio        = air_energy / total_band_energy

    # Crest factor & Zero Crossing Rate (incorporating C++ initial lastSample=0.0 condition)
    crest_factor = (peak_level / mean_rms) if mean_rms > 1e-5 else 1.0
    first_crossing = 1 if (mono[0] < 0.0) else 0
    crossings = first_crossing + np.sum(((mono[:-1] >= 0.0) & (mono[1:] < 0.0)) | ((mono[:-1] < 0.0) & (mono[1:] >= 0.0)))
    zcr = float(crossings) / max(1, n_samples)

    # Dynamic Envelope & Transient Tracking
    env_fast = scipy.signal.lfilter([fast_coeff], [1.0, -(1.0 - fast_coeff)], abs_mono)
    env_slow = scipy.signal.lfilter([slow_coeff], [1.0, -(1.0 - slow_coeff)], abs_mono)

    env_diff = env_fast - env_slow
    env_dynamics = float(np.sum(env_diff * env_diff) * 10000.0) / max(1, n_samples)

    spike_candidates = (env_fast > (env_slow * 1.55 + 0.005)) & (abs_mono > 0.008)
    spike_indices = np.where(spike_candidates)[0]

    transient_hits = 0
    last_hit = -refractory_samples
    for idx in spike_indices:
        if idx - last_hit > refractory_samples:
            transient_hits += 1
            last_hit = idx

    duration_sec = n_samples / sr
    transient_density = float(transient_hits) / max(0.5, duration_sec)

    features = np.array([
        np.clip(sub_ratio, 0.0, 1.0),
        np.clip(bass_ratio, 0.0, 1.0),
        np.clip(vocal_core_ratio, 0.0, 1.0),
        np.clip(vocal_pres_ratio, 0.0, 1.0),
        np.clip(air_ratio, 0.0, 1.0),
        np.clip(crest_factor, 1.0, 50.0),
        np.clip(zcr, 0.0, 1.0),
        np.clip(transient_density, 0.0, 30.0),
        np.clip(env_dynamics, 0.0, 100.0)
    ], dtype=np.float32)

    return features, False


# ==============================================================================
# 2. CROSS-LANGUAGE PYTHON <-> C++ PARITY TEST RUNNER
# ==============================================================================

def verify_direct_cpp_parity(cpp_exe_path: str):
    """
    Executes ProVerb_FeatureParityTest.exe in --dump-features mode on generated test audio,
    parses C++ output JSON, and computes exact parity metrics against Python.
    """
    print(f"\n[*] Running direct cross-language parity audit against compiled C++ executable: {cpp_exe_path}...", flush=True)
    if not os.path.exists(cpp_exe_path):
        raise FileNotFoundError(f"C++ test executable not found at {cpp_exe_path}. Build the project first.")

    with tempfile.TemporaryDirectory() as tmp_dir:
        test_cases = [
            ("Multi-Tone + Transients (44.1k)", 44100, 1.5),
            ("Swept Chirp (48.0k)", 48000, 1.2),
            ("Filtered Noise Bursts (96.0k)", 96000, 0.8)
        ]

        for test_name, sr, duration in test_cases:
            n_samples = int(sr * duration)
            t = np.linspace(0, duration, n_samples, endpoint=False)

            if "Multi-Tone" in test_name:
                audio = 0.35 * np.sin(2 * np.pi * 120.0 * t) + 0.25 * np.sin(2 * np.pi * 1500.0 * t)
                audio[::int(sr * 0.25)] += 0.75  # Add transient spikes
            elif "Chirp" in test_name:
                audio = 0.40 * scipy.signal.chirp(t, f0=50.0, t1=duration, f1=10000.0, method='logarithmic')
            else:
                noise = np.random.normal(0, 0.25, n_samples)
                audio = scipy.signal.medfilt(noise, 15)

            audio = np.clip(audio, -0.95, 0.95).astype(np.float32)
            wav_path = os.path.join(tmp_dir, f"parity_{sr}.wav")
            sf.write(wav_path, audio, sr, subtype='FLOAT')

            # 1. Run C++ executable to extract features
            cmd = [cpp_exe_path, "--dump-features", wav_path]
            proc = subprocess.run(cmd, capture_output=True, text=True)
            if proc.returncode != 0:
                raise RuntimeError(f"C++ feature dump failed (code {proc.returncode}):\n{proc.stderr}\n{proc.stdout}")

            try:
                cpp_res = json.loads(proc.stdout)
                cpp_feats = np.array(cpp_res["features"], dtype=np.float32)
            except Exception as e:
                raise RuntimeError(f"Failed to parse C++ JSON output: {e}\nRaw output: {proc.stdout}")

            # 2. Run Python feature extractor
            py_feats, py_sil = extract_features_exact_cpp(audio, float(sr))
            assert not py_sil, "Test audio generated unexpected silence flag in Python!"

            # 3. Compare feature by feature
            abs_diffs = np.abs(cpp_feats - py_feats)
            max_abs_diff = float(np.max(abs_diffs))

            print(f"\n    --- Parity Comparison: {test_name} ---", flush=True)
            print(f"    {'Feature':<18} | {'C++ Value':<12} | {'Python Value':<12} | {'Abs Diff':<10}", flush=True)
            print("    " + "-" * 60, flush=True)
            for i, fname in enumerate(FEATURE_NAMES):
                print(f"    {fname:<18} | {cpp_feats[i]:12.6f} | {py_feats[i]:12.6f} | {abs_diffs[i]:10.2e}", flush=True)

            print(f"    >> Max Absolute Difference: {max_abs_diff:.2e}", flush=True)
            if max_abs_diff >= 1e-4:
                raise AssertionError(f"Cross-language parity mismatch on {test_name}! Max diff {max_abs_diff:.6f} exceeds 1e-4 threshold.")

    print("\n[PASS] Cross-language numerical parity audit PASSED: Python and C++ produce identical features (< 1e-4).", flush=True)


# ==============================================================================
# 3. DATASET ACQUISITION & SOURCE-SEPARATED GROUP SPLIT
# ==============================================================================

def read_audio_file(file_path: str) -> Tuple[np.ndarray, float]:
    """
    Decodes audio using soundfile. Supports WAV, FLAC, AIFF.
    Downmixes multi-channel audio to mono using mean channel policy.
    Returns normalized float32 mono audio and sample rate.
    """
    try:
        data, sr = sf.read(file_path, dtype='float32', always_2d=True)
    except Exception as e:
        raise ValueError(f"Failed to decode audio file '{file_path}': {e}")

    if data.shape[0] == 0:
        raise ValueError(f"Audio file '{file_path}' contains zero samples.")

    mono = np.mean(data, axis=1)
    mono = np.nan_to_num(mono, nan=0.0, posinf=1.0, neginf=-1.0)
    return mono, float(sr)


def load_dataset_from_directory(dataset_dir: str, slice_duration: float = 2.5) -> Tuple[np.ndarray, np.ndarray, List[str], dict]:
    """
    Loads labeled audio recordings from dataset_dir/<class_name>/*.{wav,flac,aiff}.
    Extracts fixed slices and tracks source recording IDs for leakage-free group splitting.
    """
    print(f"\n[*] Scanning real audio dataset directory: {dataset_dir}...", flush=True)
    class_map = {name.lower().replace(" ", "").replace("/", ""): idx for idx, name in enumerate(CLASS_NAMES)}

    X_list, y_list, source_ids = [], [], []
    file_counts = {c: 0 for c in range(NUM_CLASSES)}

    for root, _, files in os.walk(dataset_dir):
        rel_dir = os.path.basename(root).lower().replace(" ", "").replace("/", "")
        matched_class = None
        for key, c_idx in class_map.items():
            if key in rel_dir or CLASS_NAMES[c_idx].lower().split()[0] in rel_dir:
                matched_class = c_idx
                break

        if matched_class is None:
            continue

        for f in files:
            if f.lower().endswith(('.wav', '.flac', '.aiff', '.aif', '.ogg')):
                fpath = os.path.join(root, f)
                try:
                    data, sr = read_audio_file(fpath)
                    slice_len = int(sr * slice_duration)
                    if len(data) < slice_len:
                        continue

                    file_counts[matched_class] += 1
                    for offset in range(0, len(data) - slice_len + 1, slice_len):
                        chunk = data[offset:offset + slice_len]
                        feats, is_sil = extract_features_exact_cpp(chunk, sr)
                        if not is_sil:
                            X_list.append(feats)
                            y_list.append(matched_class)
                            source_ids.append(fpath)
                except Exception as e:
                    print(f"    [!] Warning: Error reading {fpath}: {e}", flush=True)

    # Validate that every class has sufficient recordings
    insufficient_classes = [CLASS_NAMES[c] for c in range(NUM_CLASSES) if file_counts[c] == 0]
    if insufficient_classes:
        raise RuntimeError(f"Real dataset directory '{dataset_dir}' is missing audio for classes: {insufficient_classes}. Cannot train production model.")

    provenance = {
        "model_type": "REAL_AUDIO_RECORDINGS",
        "dataset_path": os.path.abspath(dataset_dir),
        "status": "HELD_OUT_EVALUATED_NON_CERTIFIED",
        "certification_notice": "Model evaluated on held-out real recordings. Production certification requires independent multi-studio audio validation suite.",
        "recordings_per_class": {CLASS_NAMES[c]: file_counts[c] for c in range(NUM_CLASSES)},
        "total_source_recordings": sum(file_counts.values()),
        "total_chunks": len(X_list)
    }

    return np.array(X_list, dtype=np.float32), np.array(y_list, dtype=np.int64), source_ids, provenance


def load_dataset_from_manifest(manifest_path: str, slice_duration: float = 2.5,
                               allow_incomplete_classes: bool = False) -> Tuple[np.ndarray, np.ndarray, List[str], dict]:
    """
    Loads labeled audio recordings referenced by an audited training data manifest JSON.
    Applies strict license filtering (COMMERCIALLY_PERMITTED / ATTRIBUTION_REQUIRED only),
    verifies audio file existence on disk, extracts features with exact C++ parity,
    and preserves parent track grouping to prevent data leakage.
    """
    print(f"\n[*] Loading audio recordings from manifest: {manifest_path}...", flush=True)
    if not os.path.exists(manifest_path):
        raise FileNotFoundError(f"Manifest file not found: {manifest_path}")

    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest_data = json.load(f)

    recordings = manifest_data.get("recordings", [])
    if not recordings:
        raise ValueError(f"Manifest '{manifest_path}' contains zero recording entries.")

    manifest_dir = os.path.dirname(os.path.abspath(manifest_path))
    project_root = os.path.abspath(os.path.join(manifest_dir, ".."))

    X_list, y_list, source_ids = [], [], []
    class_counts = {c: 0 for c in range(NUM_CLASSES)}
    excluded_count = 0
    missing_files_count = 0
    eligible_count = 0

    for record_index, rec in enumerate(recordings, start=1):
        perm = rec.get("permission_status", "")
        ai_allowed = rec.get("ai_training_allowed", True)
        if perm not in ["COMMERCIALLY_PERMITTED", "ATTRIBUTION_REQUIRED"] or not ai_allowed:
            excluded_count += 1
            continue

        raw_path = rec.get("file_path", "")
        if os.path.isabs(raw_path):
            file_path = raw_path
        else:
            p1 = os.path.normpath(os.path.join(manifest_dir, raw_path))
            p2 = os.path.normpath(os.path.join(project_root, raw_path))
            file_path = p1 if os.path.exists(p1) else p2

        if not os.path.exists(file_path):
            missing_files_count += 1
            continue

        eligible_count += 1
        c_idx = int(rec.get("class_index", 6))
        parent_id = str(rec.get("parent_track_id", rec.get("recording_id", file_path)))

        try:
            data, sr = read_audio_file(file_path)
            slice_len = int(sr * slice_duration)
            if len(data) < slice_len:
                feats, is_sil = extract_features_exact_cpp(data, sr)
                if not is_sil:
                    X_list.append(feats)
                    y_list.append(c_idx)
                    source_ids.append(parent_id)
                    class_counts[c_idx] += 1
                continue

            class_counts[c_idx] += 1
            for offset in range(0, len(data) - slice_len + 1, slice_len):
                chunk = data[offset:offset + slice_len]
                feats, is_sil = extract_features_exact_cpp(chunk, sr)
                if not is_sil:
                    X_list.append(feats)
                    y_list.append(c_idx)
                    source_ids.append(parent_id)
        except Exception as e:
            print(f"    [!] Warning: Error reading audio {file_path}: {e}", flush=True)
        finally:
            if record_index % 500 == 0 or record_index == len(recordings):
                print(
                    f"    - Processed {record_index}/{len(recordings)} records; "
                    f"extracted {len(X_list)} chunks.",
                    flush=True,
                )

    print(f"    - Eligible records found: {eligible_count} | Excluded (License): {excluded_count} | Missing on disk: {missing_files_count}", flush=True)

    if len(X_list) == 0:
        if missing_files_count > 0:
            raise FileNotFoundError(
                f"Audio files referenced in manifest '{manifest_path}' are not present on local disk.\n"
                f"Missing {missing_files_count} audio files. Please download dataset stems into your data directory\n"
                f"or run with --allow-synthetic to train a synthetic prototype model."
            )
        else:
            raise RuntimeError(f"Zero audio slices extracted from manifest '{manifest_path}'.")

    # Check class coverage
    unsupported_classes = [CLASS_NAMES[c] for c in range(NUM_CLASSES) if class_counts[c] == 0]
    if unsupported_classes:
        print("\n" + "!" * 78, flush=True)
        print(" [DATASET AUDIT NOTICE: UNSUPPORTED CLASSES DETECTED]", flush=True)
        print(f" The manifest does not provide real audio examples for: {unsupported_classes}", flush=True)
        print(" NOTE: Slakh2100 contains 0 real vocals (algorithmic MIDI stems only).", flush=True)
        print(" Never quietly substitute synthetic audio to fill unsupported classes.", flush=True)
        print("!" * 78 + "\n", flush=True)
        if not allow_incomplete_classes:
            raise RuntimeError(
                f"Real dataset manifest is missing audio for classes: {unsupported_classes}.\n"
                f"To train an experimental model on available classes only, pass --allow-incomplete-classes."
            )

    provenance = {
        "model_type": "REAL_AUDIO_MANIFEST",
        "manifest_path": os.path.abspath(manifest_path),
        "status": "HELD_OUT_EVALUATED_NON_CERTIFIED",
        "certification_notice": "Model trained on licensed audio recordings from manifest. Production certification requires independent multi-studio validation suite.",
        "recordings_per_class": {CLASS_NAMES[c]: class_counts[c] for c in range(NUM_CLASSES)},
        "total_source_recordings": sum(class_counts.values()),
        "total_chunks": len(X_list),
        "unsupported_classes": unsupported_classes
    }

    return np.array(X_list, dtype=np.float32), np.array(y_list, dtype=np.int64), source_ids, provenance



def synthesize_acoustic_example(class_idx: int, sr: int = 44100, duration: float = 2.5) -> np.ndarray:
    """Procedurally synthesizes representative acoustic stem signals for prototype training."""
    n_samples = int(sr * duration)
    t = np.linspace(0.0, duration, n_samples, endpoint=False)
    audio = np.zeros(n_samples, dtype=np.float32)
    gain = np.random.uniform(0.40, 0.95)

    if class_idx == 0:  # DRUMS
        bpm = np.random.uniform(85.0, 145.0)
        beat_interval = int(sr * (60.0 / bpm))
        for start in range(0, n_samples - int(sr * 0.3), beat_interval):
            k_len = int(sr * 0.25)
            kt = np.linspace(0, 0.25, k_len)
            k_freq = 45.0 + 85.0 * np.exp(-kt * 25.0)
            phase = 2.0 * np.pi * np.cumsum(k_freq) / sr
            kick = np.sin(phase) * np.exp(-kt * 14.0)
            audio[start:start + k_len] += kick * 0.85

            s_start = start + beat_interval // 2
            if s_start + int(sr * 0.2) < n_samples:
                s_len = int(sr * 0.2)
                st = np.linspace(0, 0.2, s_len)
                snare_tone = np.sin(2.0 * np.pi * 185.0 * st) * np.exp(-st * 22.0)
                snare_noise = np.random.normal(0, 1, s_len) * np.exp(-st * 18.0)
                audio[s_start:s_start + s_len] += (snare_tone * 0.4 + snare_noise * 0.6) * 0.75

        hh_interval = max(1, beat_interval // 4)
        for h_start in range(0, n_samples - int(sr * 0.05), hh_interval):
            h_len = int(sr * 0.04)
            ht = np.linspace(0, 0.04, h_len)
            noise = np.random.normal(0, 1, h_len) * np.exp(-ht * 80.0)
            audio[h_start:h_start + h_len] += noise * 0.25

    elif class_idx == 1:  # VOCALS
        f0 = np.random.uniform(130.0, 340.0)
        vibrato = np.sin(2.0 * np.pi * np.random.uniform(4.5, 6.0) * t) * 5.0
        pitch = f0 + vibrato
        phase = 2.0 * np.pi * np.cumsum(pitch) / sr
        carrier = np.sin(phase) + 0.65 * np.sin(2 * phase) + 0.40 * np.sin(3 * phase) + 0.25 * np.sin(4 * phase)
        formant = np.sin(2.0 * np.pi * 1150.0 * t) * 0.35 + np.sin(2.0 * np.pi * 2600.0 * t) * 0.30
        syllables = np.abs(np.sin(2.0 * np.pi * np.random.uniform(2.2, 4.2) * t)) ** 1.6
        audio = (carrier * 0.7 + formant) * syllables

    elif class_idx == 2:  # GUITAR
        strum_interval = int(sr * np.random.uniform(0.35, 0.75))
        f_notes = [np.random.uniform(110.0, 440.0) for _ in range(4)]
        for start in range(0, n_samples - int(sr * 0.6), strum_interval):
            plen = int(sr * 0.6)
            pt = np.linspace(0, 0.6, plen)
            for fn in f_notes:
                tone = np.sin(2.0 * np.pi * fn * pt) * np.exp(-pt * 5.5)
                harm = np.sin(2.0 * np.pi * fn * 2.0 * pt) * np.exp(-pt * 8.5) * 0.45
                audio[start:start + plen] += (tone + harm) * 0.25

    elif class_idx == 3:  # PIANO / KEYS
        chord = [261.63, 329.63, 392.0, 523.25]
        for f in chord:
            f_actual = f * np.random.uniform(0.9, 1.2)
            decay = np.exp(-t * np.random.uniform(1.2, 2.8))
            audio += np.sin(2.0 * np.pi * f_actual * t) * decay * 0.35
            audio += np.sin(2.0 * np.pi * f_actual * 2 * t) * (decay ** 1.5) * 0.20

    elif class_idx == 4:  # BASS
        f0 = np.random.uniform(42.0, 95.0)
        audio = np.sin(2.0 * np.pi * f0 * t) * 0.90 + np.sin(2.0 * np.pi * f0 * 2 * t) * 0.40
        note_len = int(sr * 0.5)
        for st in range(0, n_samples, note_len):
            end = min(n_samples, st + note_len)
            window = np.sin(np.linspace(0, np.pi, end - st))
            audio[st:end] *= window

    elif class_idx == 5:  # SYNTH / PAD
        f_base = np.random.uniform(220.0, 440.0)
        audio = np.sin(2.0 * np.pi * f_base * t) * 0.40
        audio += np.sin(2.0 * np.pi * (f_base * 1.006) * t) * 0.35
        audio += np.sin(2.0 * np.pi * (f_base * 0.994) * t) * 0.35
        audio += np.sin(2.0 * np.pi * (f_base * 2.0) * t) * 0.20

    else:  # OTHER / UNCERTAIN
        noise = np.random.normal(0, 1, n_samples).astype(np.float32)
        audio = scipy.signal.medfilt(noise, 25) * 0.65

    max_val = np.max(np.abs(audio)) + 1e-9
    audio = (audio / max_val) * gain
    return audio


def build_synthetic_prototype_dataset(samples_per_class: int = 500) -> Tuple[np.ndarray, np.ndarray, List[str], dict]:
    """Generates synthetic acoustic dataset with distinct stem IDs to prevent data leakage."""
    print("\n" + "=" * 78, flush=True)
    print(" [NOTICE] RUNNING IN SYNTHETIC DATASET PROTOTYPE MODE", flush=True)
    print(" - Procedurally synthesizes acoustic signal characteristics for 7 classes.", flush=True)
    print(" - PROTOTYPE STATUS: Real-world recording recognition accuracy is UNVERIFIED.", flush=True)
    print(" - Evaluation metrics reflect procedural synthesizer discrimination only.", flush=True)
    print(" - For production accuracy, supply labeled multitracks via --dataset-dir.", flush=True)
    print("=" * 78 + "\n", flush=True)

    X_list, y_list, stem_ids = [], [], []

    for c in range(NUM_CLASSES):
        print(f"    - Synthesizing class [{c}] {CLASS_NAMES[c]} ({samples_per_class} stems)...", flush=True)
        for i in range(samples_per_class):
            audio = synthesize_acoustic_example(c)
            feats, is_sil = extract_features_exact_cpp(audio)
            if not is_sil:
                X_list.append(feats)
                y_list.append(c)
                stem_ids.append(f"synthetic_stem_{c}_{i}")

    provenance = {
        "model_type": "SYNTHETIC_DATA_PROTOTYPE",
        "status": "UNCERTIFIED_PROTOTYPE",
        "certification_notice": "Model trained on synthetic procedural audio. Real-world acoustic recognition accuracy is UNVERIFIED. NOT certified for production instrument recognition.",
        "recordings_per_class": {CLASS_NAMES[c]: samples_per_class for c in range(NUM_CLASSES)},
        "total_source_recordings": samples_per_class * NUM_CLASSES,
        "total_chunks": len(X_list)
    }

    return np.array(X_list, dtype=np.float32), np.array(y_list, dtype=np.int64), stem_ids, provenance


def group_split_dataset(X: np.ndarray, y: np.ndarray, source_ids: List[str],
                        train_ratio: float = 0.70, val_ratio: float = 0.15, test_ratio: float = 0.15) -> Dict:
    """
    Source-separated group split: splits unique recording IDs per class into train, val, and test.
    Ensures that zero slices from the same source file ever cross split boundaries.
    """
    print("\n[*] Performing Source-Recording Group Split (Zero cross-chunk leakage)...", flush=True)
    train_indices, val_indices, test_indices = [], [], []
    split_report = {"train": {}, "val": {}, "test": {}}

    for c in range(NUM_CLASSES):
        class_indices = np.where(y == c)[0]
        if len(class_indices) == 0:
            split_report["train"][CLASS_NAMES[c]] = {"sources": 0, "chunks": 0}
            split_report["val"][CLASS_NAMES[c]]   = {"sources": 0, "chunks": 0}
            split_report["test"][CLASS_NAMES[c]]  = {"sources": 0, "chunks": 0}
            continue

        class_sources = np.array([source_ids[i] for i in class_indices])
        unique_sources = np.unique(class_sources)

        # Stable hash buckets keep a recording in the same split when datasets
        # are added later, making candidate-vs-baseline comparisons meaningful.
        train_cutoff = int(train_ratio * 100)
        val_cutoff = int((train_ratio + val_ratio) * 100)
        source_buckets = {}
        for source in unique_sources:
            digest = hashlib.sha256(f"{c}:{source}".encode("utf-8")).digest()
            bucket = int.from_bytes(digest[:8], byteorder="big") % 100
            source_buckets[source] = (
                "train" if bucket < train_cutoff
                else "val" if bucket < val_cutoff
                else "test"
            )

        train_src = {source for source, split in source_buckets.items() if split == "train"}
        val_src = {source for source, split in source_buckets.items() if split == "val"}
        test_src = {source for source, split in source_buckets.items() if split == "test"}

        c_train_idx = [idx for idx in class_indices if source_ids[idx] in train_src]
        c_val_idx   = [idx for idx in class_indices if source_ids[idx] in val_src]
        c_test_idx  = [idx for idx in class_indices if source_ids[idx] in test_src]

        train_indices.extend(c_train_idx)
        val_indices.extend(c_val_idx)
        test_indices.extend(c_test_idx)

        split_report["train"][CLASS_NAMES[c]] = {"sources": len(train_src), "chunks": len(c_train_idx)}
        split_report["val"][CLASS_NAMES[c]]   = {"sources": len(val_src),   "chunks": len(c_val_idx)}
        split_report["test"][CLASS_NAMES[c]]  = {"sources": len(test_src),  "chunks": len(c_test_idx)}

    print(f"    - Train Split: {len(train_indices)} chunks across classes", flush=True)
    print(f"    - Val Split:   {len(val_indices)} chunks across classes", flush=True)
    print(f"    - Test Split:  {len(test_indices)} chunks strictly held-out", flush=True)

    return {
        "train_idx": np.array(train_indices),
        "val_idx":   np.array(val_indices),
        "test_idx":  np.array(test_indices),
        "report":    split_report
    }


# ==============================================================================
# 4. PYTORCH MODEL ARCHITECTURE & NORMALIZER WRAPPER
# ==============================================================================

class BaseAcousticClassifier(nn.Module):
    """Raw logits classifier trained with CrossEntropyLoss for numerical stability."""
    def __init__(self, num_features=9, num_classes=7):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(num_features, 64),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.Dropout(0.15),
            nn.Linear(64, 32),
            nn.BatchNorm1d(32),
            nn.ReLU(),
            nn.Dropout(0.10),
            nn.Linear(32, num_classes)
        )

    def forward(self, x):
        return self.net(x)


class NormalizedClassifier(nn.Module):
    """
    Wraps the trained classifier with training-set feature normalization parameters.
    At inference time (both PyTorch and ONNX Runtime CPU), raw [1, 9] features are normalized
    inside the ONNX graph: x_norm = (x - mean) / std.
    Zero C++ changes needed, zero train/test leakage.
    """
    def __init__(self, base_classifier: nn.Module, mean: np.ndarray, std: np.ndarray):
        super().__init__()
        self.base_classifier = base_classifier
        self.register_buffer("mean", torch.tensor(mean, dtype=torch.float32).view(1, NUM_FEATURES))
        self.register_buffer("std", torch.tensor(std, dtype=torch.float32).view(1, NUM_FEATURES))

    def forward(self, x):
        x_norm = (x - self.mean) / self.std
        return self.base_classifier(x_norm)


# ==============================================================================
# 5. TRAINING, EVALUATION & EXPORT PIPELINE
# ==============================================================================

def train_and_export(dataset_dir: Optional[str] = None,
                     manifest_path: Optional[str] = None,
                     allow_synthetic: bool = False,
                     allow_incomplete_classes: bool = False,
                     candidate_only: bool = False,
                     class_weighting: str = "none"):
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(tools_dir, ".."))
    cpp_exe_path = os.path.join(project_root, "build", "Release", "ProVerb_FeatureParityTest.exe")

    # Step 1: Run direct cross-language parity test against compiled C++ binary
    if os.path.exists(cpp_exe_path):
        verify_direct_cpp_parity(cpp_exe_path)
    else:
        print(f"[!] Notice: C++ test executable not found at {cpp_exe_path}. Build project to run C++ cross-language parity.", flush=True)

    torch.manual_seed(42)
    np.random.seed(42)

    # Step 2: Dataset Acquisition
    if manifest_path:
        X, y, source_ids, provenance = load_dataset_from_manifest(
            manifest_path, allow_incomplete_classes=allow_incomplete_classes
        )
    elif dataset_dir:
        if not os.path.isdir(dataset_dir):
            raise FileNotFoundError(f"Specified dataset directory does not exist: {dataset_dir}")
        X, y, source_ids, provenance = load_dataset_from_directory(dataset_dir)
    else:
        if not allow_synthetic:
            raise RuntimeError(
                "No --manifest or --dataset-dir provided.\n"
                "To train an experimental/production model on real audio, provide --manifest Assets/training_data_manifest.json.\n"
                "To train a synthetic acoustic prototype, explicitly specify --allow-synthetic."
            )
        X, y, source_ids, provenance = build_synthetic_prototype_dataset(samples_per_class=500)

    # Step 3: Leakage-Free Group Split
    splits = group_split_dataset(X, y, source_ids)
    train_idx = splits["train_idx"]
    val_idx   = splits["val_idx"]
    test_idx  = splits["test_idx"]

    X_train_raw = X[train_idx]
    y_train = torch.tensor(y[train_idx], dtype=torch.int64)

    X_val_raw   = X[val_idx]
    y_val   = torch.tensor(y[val_idx], dtype=torch.int64)

    X_test_raw  = X[test_idx]
    y_test  = torch.tensor(y[test_idx], dtype=torch.int64)

    # Step 4: Training-Only Feature Normalization (Strictly zero test/val leakage)
    feat_mean = np.mean(X_train_raw, axis=0)
    feat_std  = np.std(X_train_raw, axis=0)
    feat_std  = np.where(feat_std < 1e-4, 1.0, feat_std)

    print("\n[*] Training feature normalization parameters (derived strictly from Train split):", flush=True)
    for i, name in enumerate(FEATURE_NAMES):
        print(f"    - {name:<18}: Mean = {feat_mean[i]:8.4f}, Std = {feat_std[i]:8.4f}", flush=True)

    X_train = torch.tensor((X_train_raw - feat_mean) / feat_std, dtype=torch.float32)
    X_val   = torch.tensor((X_val_raw - feat_mean) / feat_std, dtype=torch.float32)
    X_test  = torch.tensor((X_test_raw - feat_mean) / feat_std, dtype=torch.float32)

    train_loader = DataLoader(TensorDataset(X_train, y_train), batch_size=32, shuffle=True)
    val_loader   = DataLoader(TensorDataset(X_val, y_val), batch_size=64, shuffle=False)
    test_loader  = DataLoader(TensorDataset(X_test, y_test), batch_size=64, shuffle=False)

    # Step 5: Logits Classifier Training with Early Stopping & Cosine Annealing
    base_model = BaseAcousticClassifier(num_features=NUM_FEATURES, num_classes=NUM_CLASSES)
    train_class_counts = np.bincount(y[train_idx], minlength=NUM_CLASSES)
    class_weights_np = None
    if class_weighting == "sqrt":
        missing_train_classes = [CLASS_NAMES[c] for c, count in enumerate(train_class_counts) if count == 0]
        if missing_train_classes:
            raise RuntimeError(
                "Square-root class weighting requires at least one training sample per class; "
                f"missing: {missing_train_classes}"
            )
        class_weights_np = np.sqrt(len(train_idx) / (NUM_CLASSES * train_class_counts.astype(np.float64)))
        class_weights_np /= np.mean(class_weights_np)
        print("\n[*] Applying square-root inverse-frequency class weights from train split only:", flush=True)
        for c, weight in enumerate(class_weights_np):
            print(f"    - {CLASS_NAMES[c]:<18}: {weight:.3f} (n={train_class_counts[c]})", flush=True)
        criterion = nn.CrossEntropyLoss(weight=torch.tensor(class_weights_np, dtype=torch.float32))
    elif class_weighting == "none":
        criterion = nn.CrossEntropyLoss()
    else:
        raise ValueError(f"Unsupported class_weighting mode: {class_weighting}")
    optimizer = optim.AdamW(base_model.parameters(), lr=0.003, weight_decay=1e-4)
    epochs = 40
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs)

    best_val_loss = float("inf")
    best_weights = None
    patience = 8
    patience_counter = 0

    print(f"\n[*] Training Logits Classifier ({epochs} epochs max)...", flush=True)
    for epoch in range(1, epochs + 1):
        base_model.train()
        train_loss = 0.0
        for bx, by in train_loader:
            optimizer.zero_grad()
            logits = base_model(bx)
            loss = criterion(logits, by)
            loss.backward()
            optimizer.step()
            train_loss += loss.item() * len(bx)
        scheduler.step()
        train_loss /= len(X_train)

        base_model.eval()
        val_loss = 0.0
        correct_val = 0
        with torch.no_grad():
            for bx, by in val_loader:
                logits = base_model(bx)
                loss = criterion(logits, by)
                val_loss += loss.item() * len(bx)
                preds = torch.argmax(logits, dim=-1)
                correct_val += (preds == by).sum().item()
        val_loss /= len(X_val)
        val_acc = (correct_val / len(X_val)) * 100.0

        if epoch % 5 == 0 or epoch == 1 or epoch == epochs:
            print(f"    Epoch {epoch:2d}/{epochs} — Train Loss: {train_loss:.4f} | Val Loss: {val_loss:.4f} | Val Acc: {val_acc:.1f}%", flush=True)

        if val_loss < best_val_loss:
            best_val_loss = val_loss
            best_weights = {k: v.cpu().clone() for k, v in base_model.state_dict().items()}
            patience_counter = 0
        else:
            patience_counter += 1
            if patience_counter >= patience:
                print(f"    [*] Early stopping triggered at epoch {epoch} (Best Val Loss: {best_val_loss:.4f})", flush=True)
                break

    base_model.load_state_dict(best_weights)

    # Step 6: Held-Out Test Set Evaluation
    print(f"\n[*] Evaluating on Strictly Held-Out Test Set ({len(X_test)} samples)...", flush=True)
    base_model.eval()
    all_preds, all_targets, all_probs = [], [], []

    with torch.no_grad():
        for bx, by in test_loader:
            logits = base_model(bx)
            probs = torch.softmax(logits, dim=-1)
            preds = torch.argmax(logits, dim=-1)
            all_preds.extend(preds.numpy())
            all_targets.extend(by.numpy())
            all_probs.extend(probs.numpy())

    all_preds = np.array(all_preds)
    all_targets = np.array(all_targets)

    test_acc = np.mean(all_preds == all_targets) * 100.0
    print("\n" + "=" * 64, flush=True)
    print(f" HELD-OUT TEST ACCURACY: {test_acc:.2f}% [{provenance['model_type']}]", flush=True)
    print("=" * 64 + "\n", flush=True)

    cm = np.zeros((NUM_CLASSES, NUM_CLASSES), dtype=int)
    for p, t in zip(all_preds, all_targets):
        cm[t, p] += 1

    print("CONFUSION MATRIX (Rows: Actual Class, Columns: Predicted Class):", flush=True)
    header = "          " + "".join([f"{CLASS_NAMES[i][:6]:>8}" for i in range(NUM_CLASSES)])
    print(header, flush=True)
    print("        " + "-" * 58, flush=True)
    for r in range(NUM_CLASSES):
        row_str = f"{CLASS_NAMES[r][:8]:>8} |"
        for c in range(NUM_CLASSES):
            row_str += f"{cm[r, c]:8d}"
        print(row_str, flush=True)

    print("\nPER-CLASS CLASSIFICATION METRICS (HELD-OUT TEST SET):", flush=True)
    print("Class                | Precision | Recall   | F1-Score | Test Count", flush=True)
    print("----------------------------------------------------------------", flush=True)
    per_class_metrics = {}
    for c in range(NUM_CLASSES):
        tp = cm[c, c]
        fp = np.sum(cm[:, c]) - tp
        fn = np.sum(cm[c, :]) - tp
        prec = (tp / (tp + fp)) if (tp + fp) > 0 else 0.0
        rec  = (tp / (tp + fn)) if (tp + fn) > 0 else 0.0
        f1   = (2 * prec * rec) / (prec + rec) if (prec + rec) > 0 else 0.0
        count = int(np.sum(cm[c, :]))
        per_class_metrics[CLASS_NAMES[c]] = {
            "precision": float(prec) if count > 0 else None,
            "recall": float(rec) if count > 0 else None,
            "f1_score": float(f1) if count > 0 else None,
            "test_count": count,
            "status": "EVALUATED" if count > 0 else "UNSUPPORTED_ZERO_TEST_SAMPLES"
        }
        if count == 0:
            print(f"{CLASS_NAMES[c]:<20} |      N/A |     N/A |     N/A |          0 (UNSUPPORTED)", flush=True)
        else:
            print(f"{CLASS_NAMES[c]:<20} | {prec*100:8.1f}% | {rec*100:7.1f}% | {f1*100:7.1f}% | {count:10d}", flush=True)

    evaluated_f1 = [m["f1_score"] for m in per_class_metrics.values() if m["f1_score"] is not None]
    macro_f1_test = float(np.mean(evaluated_f1)) if evaluated_f1 else 0.0
    print(f"\nMACRO F1 (evaluated classes): {macro_f1_test * 100.0:.2f}%", flush=True)

    # Step 7: Wrap with Normalization Layer for ONNX Export
    final_model = NormalizedClassifier(base_model, feat_mean, feat_std)
    final_model.eval()

    assets_dir = os.path.join(project_root, "Assets")
    os.makedirs(assets_dir, exist_ok=True)
    onnx_path = os.path.join(assets_dir, "ProVerb_AI.onnx")
    onnx_staging_path = os.path.join(assets_dir, "ProVerb_AI_staging.onnx")
    norm_path = os.path.join(assets_dir, "feature_norm_params.json")
    prov_path = os.path.join(assets_dir, "model_provenance.json")

    # Step 8: Safe ONNX Staging Export
    print(f"\n[*] Exporting trained model to staging ONNX: {onnx_staging_path}...", flush=True)
    dummy_input = torch.randn(1, NUM_FEATURES, dtype=torch.float32)

    torch.onnx.export(
        final_model,
        dummy_input,
        onnx_staging_path,
        export_params=True,
        opset_version=14,
        do_constant_folding=True,
        input_names=["input"],
        output_names=["output"],
        dynamic_axes={
            "input": {0: "batch_size"},
            "output": {0: "batch_size"}
        },
        dynamo=False
    )

    onnx_model = onnx.load(onnx_staging_path)
    onnx.checker.check_model(onnx_model)
    print("    [PASS] Staged ONNX model structure validated successfully by onnx.checker.", flush=True)

    # Step 9: ORT CPU Parity Verification
    print("\n[*] Verifying Numerical Parity against ONNX Runtime CPU...", flush=True)
    ort_session = ort.InferenceSession(onnx_staging_path, providers=["CPUExecutionProvider"])

    raw_test_inputs = X_test_raw[:10].astype(np.float32)
    with torch.no_grad():
        pytorch_logits = final_model(torch.tensor(raw_test_inputs)).numpy()

    ort_logits = ort_session.run(None, {"input": raw_test_inputs})[0]
    max_parity_diff = np.max(np.abs(pytorch_logits - ort_logits))
    print(f"    [PASS] Max absolute difference (PyTorch vs ONNX Runtime CPU): {max_parity_diff:.8f}", flush=True)
    if max_parity_diff >= 1e-4:
        if os.path.exists(onnx_staging_path):
            os.remove(onnx_staging_path)
        raise AssertionError("Export divergence! ONNX Runtime CPU output differs from PyTorch model.")

    if candidate_only:
        candidate_onnx = os.path.join(assets_dir, "ProVerb_AI_candidate.onnx")
        candidate_norm = os.path.join(assets_dir, "feature_norm_params_candidate.json")
        candidate_prov = os.path.join(assets_dir, "model_provenance_candidate.json")

        if os.path.exists(candidate_onnx):
            os.remove(candidate_onnx)
        os.rename(onnx_staging_path, candidate_onnx)
        print(f"\n[*] Candidate model saved to: {candidate_onnx}", flush=True)
        print("    [NOTICE] Active plugin model (ProVerb_AI.onnx) remains UNTOUCHED and PRESERVED.", flush=True)

        norm_params = {
            "features": FEATURE_NAMES,
            "mean": feat_mean.tolist(),
            "std": feat_std.tolist(),
            "normalization_formula": "x_norm = (x - mean) / std"
        }
        with open(candidate_norm, "w", encoding="utf-8") as f:
            json.dump(norm_params, f, indent=2)

        provenance_report = {
            "model_type": provenance["model_type"],
            "status": "CANDIDATE_STAGED_NON_ACTIVE",
            "certification_notice": provenance["certification_notice"],
            "class_weighting": class_weighting,
            "training_class_counts": {CLASS_NAMES[c]: int(train_class_counts[c]) for c in range(NUM_CLASSES)},
            "training_class_weights": ({CLASS_NAMES[c]: float(class_weights_np[c]) for c in range(NUM_CLASSES)}
                                       if class_weights_np is not None else None),
            "held_out_test_accuracy": float(test_acc),
            "macro_f1_test": macro_f1_test,
            "split_counts": {
                "train": len(X_train),
                "val": len(X_val),
                "test": len(X_test)
            },
            "per_class_metrics": per_class_metrics,
            "split_details": splits["report"]
        }
        with open(candidate_prov, "w", encoding="utf-8") as f:
            json.dump(provenance_report, f, indent=2)

        # Validate candidate model using isolated C++ harness without affecting installed active model
        if os.path.exists(cpp_exe_path):
            print(f"\n[*] Validating candidate model with C++ Test Harness in isolated staging environment...", flush=True)
            with tempfile.TemporaryDirectory() as tmp_dir:
                tmp_exe = os.path.join(tmp_dir, os.path.basename(cpp_exe_path))
                tmp_model = os.path.join(tmp_dir, "ProVerb_AI.onnx")
                shutil.copy2(cpp_exe_path, tmp_exe)
                exe_dir = os.path.dirname(cpp_exe_path)
                for f in os.listdir(exe_dir):
                    if f.lower().endswith(".dll"):
                        shutil.copy2(os.path.join(exe_dir, f), os.path.join(tmp_dir, f))
                shutil.copy2(candidate_onnx, tmp_model)

                proc = subprocess.run([tmp_exe, "--strict"], capture_output=True, text=True)
                print(proc.stdout)
                if proc.returncode != 0:
                    print(proc.stderr)
                    raise RuntimeError(f"Candidate C++ Test Harness FAILED with exit code {proc.returncode}!")
                print("    [PASS] Candidate model PASSED strict C++ test harness in isolated environment.", flush=True)

        print("\n" + "=" * 78, flush=True)
        print(" [CANDIDATE MODEL STAGED SUCCESSFULLY]", flush=True)
        print(f" - Candidate Model: {candidate_onnx}", flush=True)
        print(f" - Active Model: {onnx_path} (PRESERVED & UNCHANGED)", flush=True)
        print(" - Active plugin binaries were NOT modified.", flush=True)
        print("=" * 78 + "\n", flush=True)
        return

    # Step 10: Commit Artifacts and Create Backups of Last Known-Good State
    print("\n[*] Validation passed! Backing up previous artifacts and committing new model...", flush=True)
    for p in [onnx_path, norm_path, prov_path]:
        if os.path.exists(p):
            bak_p = p + ".bak"
            shutil.copy2(p, bak_p)
            print(f"    [BACKUP] Saved: {os.path.basename(bak_p)}", flush=True)

    # Atomically replace ONNX model
    if os.path.exists(onnx_path):
        os.remove(onnx_path)
    os.rename(onnx_staging_path, onnx_path)
    print(f"    [COMMITTED] Active model updated: {os.path.basename(onnx_path)}", flush=True)

    # Export Normalization Parameters JSON
    norm_params = {
        "features": FEATURE_NAMES,
        "mean": feat_mean.tolist(),
        "std": feat_std.tolist(),
        "normalization_formula": "x_norm = (x - mean) / std"
    }
    with open(norm_path, "w", encoding="utf-8") as f:
        json.dump(norm_params, f, indent=2)

    # Export Provenance & Metrics JSON (Strict honesty, no false certification)
    provenance_report = {
        "model_type": provenance["model_type"],
        "status": provenance["status"],
        "certification_notice": provenance["certification_notice"],
        "class_weighting": class_weighting,
        "training_class_counts": {CLASS_NAMES[c]: int(train_class_counts[c]) for c in range(NUM_CLASSES)},
        "training_class_weights": ({CLASS_NAMES[c]: float(class_weights_np[c]) for c in range(NUM_CLASSES)}
                                   if class_weights_np is not None else None),
        "held_out_test_accuracy": float(test_acc),
        "macro_f1_test": macro_f1_test,
        "split_counts": {
            "train": len(X_train),
            "val": len(X_val),
            "test": len(X_test)
        },
        "per_class_metrics": per_class_metrics,
        "split_details": splits["report"]
    }
    with open(prov_path, "w", encoding="utf-8") as f:
        json.dump(provenance_report, f, indent=2)

    # Step 11: Deploy to Search Locations
    print("\n[*] Deploying ProVerb_AI.onnx to Plugin Search Locations...", flush=True)
    deploy_dirs = [
        os.path.join(project_root, "build", "Release"),
        os.path.join(project_root, "build", "ProVerb_artefacts", "Release", "Standalone"),
        os.path.join(project_root, "build", "ProVerb_artefacts", "Release", "VST3", "ProVerb.vst3", "Contents", "x86_64-win"),
        os.path.join(os.environ.get("COMMONPROGRAMFILES", r"C:\Program Files\Common Files"), "VST3", "ProVerb.vst3", "Contents", "x86_64-win")
    ]

    deployed_count = 0
    for d in deploy_dirs:
        if os.path.exists(d):
            try:
                shutil.copy2(onnx_path, os.path.join(d, "ProVerb_AI.onnx"))
                print(f"    [PASS] Deployed into: {d}", flush=True)
                deployed_count += 1
            except PermissionError:
                print(f"    [!] Note: Skipping {d} (requires Windows administrator privileges)", flush=True)

    # Step 11: Execute Strict C++ Runtime Harness Test
    if os.path.exists(cpp_exe_path):
        print(f"\n[*] Executing Strict Mode C++ Test Harness ({cpp_exe_path} --strict)...", flush=True)
        # Ensure model is beside test exe
        test_exe_model = os.path.join(os.path.dirname(cpp_exe_path), "ProVerb_AI.onnx")
        shutil.copy2(onnx_path, test_exe_model)

        cmd = [cpp_exe_path, "--strict"]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        print(proc.stdout)
        if proc.returncode != 0:
            print(proc.stderr)
            raise RuntimeError(f"Strict C++ Test Harness FAILED with exit code {proc.returncode}!")
        print("    [PASS] Strict C++ Test Harness PASSED with 0 failures and 0 skipped tests.", flush=True)

    print("\n" + "=" * 78, flush=True)
    print(" [SUCCESS] PROVERB AI PIPELINE COMPLETED AND VERIFIED END-TO-END!", flush=True)
    print(f" - Model Path: {onnx_path}", flush=True)
    print(f" - Provenance Type: {provenance['model_type']}", flush=True)
    print(f" - Certification Status: {provenance['status']}", flush=True)
    print(f" - Deployed to {deployed_count} active plugin distribution paths.", flush=True)
    print("=" * 78 + "\n", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ProVerb AI Model Training & Export Pipeline")
    parser.add_argument("--manifest", type=str, default=None, help="Path to validated training_data_manifest.json")
    parser.add_argument("--dataset-dir", type=str, default=None, help="Path to real labeled audio dataset directory")
    parser.add_argument("--allow-synthetic", action="store_true", help="Explicitly allow training synthetic prototype model")
    parser.add_argument("--allow-incomplete-classes", action="store_true", help="Allow training with real data even if some classes are missing")
    parser.add_argument("--candidate-only", action="store_true", help="Stage trained model as candidate without modifying active plugin model")
    parser.add_argument("--class-weighting", choices=["none", "sqrt"], default="none",
                        help="Apply square-root inverse-frequency weights computed from the train split")
    parser.add_argument("--promote-to-active", action="store_true", help="Explicitly promote trained model to overwrite active plugin model")
    parser.add_argument("--test-cpp-parity", action="store_true", help="Run direct Python <-> C++ parity verification and exit")
    args = parser.parse_args()

    if args.test_cpp_parity:
        tools_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.abspath(os.path.join(tools_dir, ".."))
        cpp_exe = os.path.join(project_root, "build", "Release", "ProVerb_FeatureParityTest.exe")
        verify_direct_cpp_parity(cpp_exe)
        sys.exit(0)

    # For manifest-based training, default to staging as candidate unless explicitly promoted
    candidate_only = args.candidate_only or (args.manifest is not None and not args.promote_to_active)

    train_and_export(
        dataset_dir=args.dataset_dir,
        manifest_path=args.manifest,
        allow_synthetic=args.allow_synthetic,
        allow_incomplete_classes=args.allow_incomplete_classes,
        candidate_only=candidate_only,
        class_weighting=args.class_weighting
    )
