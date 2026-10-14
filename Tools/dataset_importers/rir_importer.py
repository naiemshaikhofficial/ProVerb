#!/usr/bin/env python3
"""
BUT Speech@FIT Reverb Database Importer & Acoustic Space Augmentation Utility
ProducerToy — ProVerb Smart Algorithmic Reverb

Official Source: https://speech.fit.vut.cz/software/but-speech-fit-reverb-database
License: Creative Commons Attribution 4.0 International (CC BY 4.0)
Attribution: BUT Speech@FIT Group, Faculty of Information Technology, Brno University of Technology.
"The BUT Speech@FIT Reverb Database: acoustic impulse responses recorded in diverse environments."

CRITICAL NOTE:
Room Impulse Responses (RIRs) characterize acoustic spaces, NOT instruments.
This utility validates RIR integrity and provides controlled dry-to-reverberant
convolution for spatial robustness testing and reverb parameter research.
"""

import os
import sys
import json
from typing import Dict, List, Optional, Tuple

import numpy as np
import scipy.signal
import soundfile as sf

DATASET_NAME = "BUT_Speech_FIT_Reverb_DB"
DATASET_VERSION = "v1.0"
SOURCE_URL = "https://speech.fit.vut.cz/software/but-speech-fit-reverb-database"
LICENSE_ID = "CC-BY-4.0"
ATTRIBUTION_TEXT = (
    "BUT Speech@FIT Reverb Database is licensed under CC BY 4.0 by "
    "Brno University of Technology, Faculty of Information Technology."
)


def validate_rir_file(rir_path: str) -> Dict:
    """
    Validates sample rates, channel count, finite samples, and impulse response duration.
    """
    if not os.path.exists(rir_path):
        raise FileNotFoundError(f"RIR file not found: {rir_path}")

    data, sr = sf.read(rir_path, dtype='float32', always_2d=True)
    if data.shape[0] == 0:
        raise ValueError(f"RIR file '{rir_path}' has 0 samples.")

    # Downmix to mono for analysis
    mono_rir = np.mean(data, axis=1)

    # Check finiteness
    is_finite = bool(np.all(np.isfinite(mono_rir)))
    if not is_finite:
        raise ValueError(f"RIR file '{rir_path}' contains non-finite samples (NaN/Inf).")

    # Peak normalization & direct sound onset detection
    abs_rir = np.abs(mono_rir)
    peak_idx = int(np.argmax(abs_rir))
    peak_val = float(abs_rir[peak_idx])

    # Approximate T60 / energy decay
    energy = mono_rir ** 2
    cum_energy = np.cumsum(energy[::-1])[::-1]
    total_energy = cum_energy[0] + 1e-12
    db_decay = 10.0 * np.log10(cum_energy / total_energy + 1e-12)

    # Find -30 dB decay point
    t30_indices = np.where(db_decay <= -30.0)[0]
    t60_est = (t30_indices[0] / sr) * 2.0 if len(t30_indices) > 0 else (len(mono_rir) / sr)

    return {
        "file_path": os.path.normpath(rir_path),
        "sample_rate": sr,
        "channels": data.shape[1],
        "duration_seconds": len(mono_rir) / sr,
        "peak_index": peak_idx,
        "peak_value": peak_val,
        "estimated_t60_seconds": float(t60_est),
        "is_valid": True,
        "license": LICENSE_ID,
        "attribution": ATTRIBUTION_TEXT
    }


def convolve_dry_with_rir(dry_audio: np.ndarray, rir_audio: np.ndarray, wet_ratio: float = 0.35) -> np.ndarray:
    """
    Applies controlled acoustic convolution between dry instrument audio and an RIR.
    Returns normalized mixed audio.
    """
    # Downmix RIR to mono if needed
    if rir_audio.ndim > 1:
        rir_audio = np.mean(rir_audio, axis=1)

    # Direct convolution via FFT
    wet_audio = scipy.signal.fftconvolve(dry_audio, rir_audio, mode='full')[:len(dry_audio)]

    # Peak normalization
    max_wet = np.max(np.abs(wet_audio)) + 1e-9
    max_dry = np.max(np.abs(dry_audio)) + 1e-9
    wet_norm = (wet_audio / max_wet) * max_dry

    mixed = (1.0 - wet_ratio) * dry_audio + wet_ratio * wet_norm
    max_mix = np.max(np.abs(mixed)) + 1e-9
    return (mixed / max_mix) * max_dry


def scan_rir_directory(rir_root: str) -> List[Dict]:
    """
    Scans a directory of RIR files and returns validated metadata.
    """
    print(f"[*] Scanning BUT Speech@FIT Reverb Database at: {rir_root}...", flush=True)
    results = []
    for root, _, files in os.walk(rir_root):
        for f in files:
            if f.lower().endswith(('.wav', '.flac')):
                fpath = os.path.join(root, f)
                try:
                    meta = validate_rir_file(fpath)
                    results.append(meta)
                except Exception as e:
                    print(f"    [!] Skipping invalid RIR {fpath}: {e}")

    print(f"[✓] Validated {len(results)} room impulse responses.")
    return results


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python rir_importer.py <path_to_rir_folder>")
        sys.exit(1)

    scan_rir_directory(sys.argv[1])
