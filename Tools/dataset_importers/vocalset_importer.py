#!/usr/bin/env python3
"""
VocalSet Dataset Importer & Singer-Disjoint Manifest Generator
ProducerToy — ProVerb Smart Algorithmic Reverb

Dataset: VocalSet: A Singing Voice Dataset
Official Source: https://doi.org/10.5281/zenodo.1193957
Authors: Julia Wilkins, Prem Seetharaman, Alison Wahl, Bryan Pardo (Northwestern University)
License: Creative Commons Attribution 4.0 International (CC BY 4.0)
Attribution: Julia Wilkins, Prem Seetharaman, Alison Wahl, Bryan Pardo.
"VocalSet: A Singing Voice Dataset." 19th International Society for Music Information Retrieval Conference (ISMIR 2018).

This importer:
1. Scans extracted VocalSet audio directory (Data/VocalSet/).
2. Extracts singer identifier (e.g., female1..female11, male1..male9 or f1..f11, m1..m9) and vocal technique.
3. Audits each audio file using soundfile (duration, sample rate, channels, finite samples).
4. Maps verified human-singing recordings to ProVerb class [1] 'Vocals'.
5. Implements a leakage-free Singer-Disjoint Split (Train / Val / Test) holding out complete singers.
6. Updates Assets/training_data_manifest.json while preserving existing dataset entries (e.g. Slakh2100).
"""

import os
import sys
import re
import json
import argparse
from collections import Counter, defaultdict
from typing import Dict, List, Optional, Tuple, Set

import soundfile as sf
import numpy as np

# Add parent directory to path to import manifest_manager
TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(TOOLS_DIR, "..", ".."))
sys.path.insert(0, os.path.abspath(os.path.join(TOOLS_DIR, "..")))
from manifest_manager import DatasetManifest, compute_file_sha256

DATASET_NAME = "VocalSet"
DATASET_VERSION = "v1.0"
SOURCE_URL = "https://doi.org/10.5281/zenodo.1193957"
LICENSE_ID = "CC-BY-4.0"
ATTRIBUTION_TEXT = (
    "VocalSet is licensed under CC BY 4.0 by Julia Wilkins, Prem Seetharaman, "
    "Alison Wahl, Bryan Pardo (Northwestern University)."
)

# Standard VocalSet 20 singers: 11 females, 9 males
# Singer-Disjoint Partition:
# Train: 14 singers (~70%)
# Val:   3 singers (~15%)
# Test:  3 singers (~15%)
SINGER_SPLIT_MAP = {
    # Female singers
    "f1": "train", "female1": "train",
    "f2": "train", "female2": "train",
    "f3": "train", "female3": "train",
    "f4": "train", "female4": "train",
    "f5": "train", "female5": "train",
    "f6": "train", "female6": "train",
    "f7": "train", "female7": "train",
    "f8": "train", "female8": "train",
    "f9": "val",   "female9": "val",
    "f10": "val",  "female10": "val",
    "f11": "test", "female11": "test",

    # Male singers
    "m1": "train", "male1": "train",
    "m2": "train", "male2": "train",
    "m3": "train", "male3": "train",
    "m4": "train", "male4": "train",
    "m5": "train", "male5": "train",
    "m6": "train", "male6": "train",
    "m7": "val",   "male7": "val",
    "m8": "test",  "male8": "test",
    "m9": "test",  "male9": "test",
}


def parse_vocalset_path(rel_path: str) -> Tuple[str, str, str]:
    """
    Extracts (singer_id, technique, descriptor) from relative path or filename.
    Handles various directory structures (by singer, by technique, or flat).
    """
    normalized = rel_path.replace("\\", "/").lower()
    parts = normalized.split("/")

    singer_id = "unknown_singer"
    technique = "singing"

    # Match singer patterns (female1..11, male1..9, f1..f11, m1..m9)
    for part in parts:
        m = re.match(r"^(female\d+|male\d+|f\d+|m\d+)$", part)
        if m:
            singer_id = m.group(1)
            break

    # If not found in directory parts, inspect filename
    filename = os.path.splitext(os.path.basename(normalized))[0]
    if singer_id == "unknown_singer":
        m = re.match(r"^(female\d+|male\d+|f\d+|m\d+)_?", filename)
        if m:
            singer_id = m.group(1)

    # Standardize singer_id (e.g. female1 -> f1 for grouping)
    norm_singer = singer_id
    if norm_singer.startswith("female"):
        norm_singer = "f" + norm_singer[6:]
    elif norm_singer.startswith("male"):
        norm_singer = "m" + norm_singer[4:]

    # Detect vocal techniques
    known_techniques = [
        "arpeggio", "arpeggios", "scale", "scales", "long_tone", "long_tones",
        "vibrato", "straight", "belt", "breathy", "vocal_fry", "lip_trill",
        "trill", "trillo", "messa_di_voce", "portamento", "slow_forte",
        "fast_forte", "slow_piano", "fast_piano", "forte", "piano"
    ]
    for part in parts:
        for t in known_techniques:
            if t in part:
                technique = t
                break

    return norm_singer, technique, filename


def import_vocalset(
    vocalset_dir: str,
    manifest_path: str,
    min_duration_sec: float = 0.3
) -> Dict:
    """
    Scans VocalSet directory, decodes and validates audio, enforces singer-disjoint splits,
    and updates the training manifest.
    """
    print("=" * 76, flush=True)
    print(" PROVERB AI — VOCALSET DATASET IMPORTER & MANIFEST BUILDER", flush=True)
    print("=" * 76, flush=True)
    print(f"[*] Audio Directory : {vocalset_dir}", flush=True)
    print(f"[*] Target Manifest : {manifest_path}\n", flush=True)

    if not os.path.exists(vocalset_dir):
        raise FileNotFoundError(f"VocalSet directory does not exist: {vocalset_dir}")

    # Load existing manifest if present to preserve Slakh / existing records
    if os.path.exists(manifest_path):
        print(f"[*] Loading existing manifest: {manifest_path}", flush=True)
        manifest = DatasetManifest.load_json(manifest_path)
        # Filter out existing VocalSet records to avoid duplicates upon re-import
        initial_recs = len(manifest.records)
        manifest.records = [r for r in manifest.records if r.get("dataset_name") != DATASET_NAME]
        removed_dupes = initial_recs - len(manifest.records)
        if removed_dupes > 0:
            print(f"    - Purged {removed_dupes} previous VocalSet entries for clean re-import.", flush=True)
    else:
        print("[*] Creating new dataset manifest...", flush=True)
        manifest = DatasetManifest(description="ProVerb Training Audio Manifest")

    # Register VocalSet source in manifest metadata
    manifest.register_dataset_source(
        dataset_name=DATASET_NAME,
        version=DATASET_VERSION,
        source_url=SOURCE_URL,
        license_id=LICENSE_ID,
        attribution=ATTRIBUTION_TEXT,
        default_permission="ATTRIBUTION_REQUIRED"
    )

    # Collect all audio files
    audio_files: List[str] = []
    for root, _, files in os.walk(vocalset_dir):
        for f in sorted(files):
            if f.startswith(".") or f.startswith("._"):
                continue
            if f.lower().endswith((".wav", ".flac", ".aif", ".aiff")):
                audio_files.append(os.path.join(root, f))

    print(f"[*] Found {len(audio_files):,} audio candidate files in {vocalset_dir}...", flush=True)

    imported_count = 0
    skipped_count = 0
    total_audio_sec = 0.0
    singer_counts = Counter()
    technique_counts = Counter()
    split_counts = Counter()
    sample_rates = Counter()
    corrupt_files = []

    for file_abs in sorted(audio_files):
        rel_to_proj = os.path.relpath(file_abs, PROJECT_ROOT)
        rel_to_vocal = os.path.relpath(file_abs, vocalset_dir)

        # Decode & validate audio with soundfile
        try:
            info = sf.info(file_abs)
            sr = float(info.samplerate)
            dur = float(info.duration)
            ch = int(info.channels)

            if dur < min_duration_sec:
                skipped_count += 1
                continue

            # Check finite audio frames on first 1 second
            data, _ = sf.read(file_abs, frames=min(int(sr), 44100), dtype="float32", always_2d=True)
            if not np.all(np.isfinite(data)):
                corrupt_files.append((rel_to_proj, "Non-finite audio data"))
                skipped_count += 1
                continue
        except Exception as e:
            corrupt_files.append((rel_to_proj, str(e)))
            skipped_count += 1
            continue

        singer_id, technique, desc = parse_vocalset_path(rel_to_vocal)

        # Enforce singer-disjoint grouped split
        split = SINGER_SPLIT_MAP.get(singer_id, "train")

        parent_track_id = f"VocalSet_singer_{singer_id}"
        recording_id = f"VocalSet_{singer_id}_{technique}_{desc}"

        rationale = (
            f"VocalSet professional singer '{singer_id}', technique '{technique}' "
            f"-> Vocals (class index 1, human singing voice)"
        )

        manifest.add_recording(
            file_path=rel_to_proj,
            dataset_name=DATASET_NAME,
            source_track_id=parent_track_id,
            recording_id=recording_id,
            class_index=1, # Vocals
            class_mapping_rationale=rationale,
            license_id=LICENSE_ID,
            attribution=ATTRIBUTION_TEXT,
            permission_status="ATTRIBUTION_REQUIRED",
            sample_rate=sr,
            duration_seconds=dur,
            channels=ch,
            creator=f"Singer {singer_id} / VocalSet (Northwestern Univ)",
            ai_training_allowed=True,
            split=split
        )

        imported_count += 1
        total_audio_sec += dur
        singer_counts[singer_id] += 1
        technique_counts[technique] += 1
        split_counts[split] += 1
        sample_rates[int(sr)] += 1

    # Save manifest
    manifest.save_json(manifest_path)

    summary = {
        "dataset_name": DATASET_NAME,
        "total_files_scanned": len(audio_files),
        "imported_clips": imported_count,
        "skipped_clips": skipped_count,
        "corrupt_files": corrupt_files,
        "total_duration_hours": round(total_audio_sec / 3600.0, 3),
        "total_duration_seconds": round(total_audio_sec, 1),
        "distinct_singers": len(singer_counts),
        "singers_breakdown": dict(singer_counts),
        "sample_rates": dict(sample_rates),
        "singer_disjoint_splits": dict(split_counts),
        "top_techniques": dict(technique_counts.most_common(10))
    }

    print("\n--- VOCALSET IMPORT SUMMARY ---")
    print(f"Total Imported Clips     : {imported_count:,}")
    print(f"Total Usable Audio       : {summary['total_duration_hours']} hours ({total_audio_sec:.1f}s)")
    print(f"Distinct Singers Identified: {len(singer_counts)}")
    print(f"Sample Rates             : {dict(sample_rates)}")
    print("\nSinger-Disjoint Split Allocation:")
    for spl, cnt in sorted(split_counts.items()):
        print(f"  - {spl:<10} : {cnt:,} clips ({cnt / max(1, imported_count) * 100:.1f}%)")
    print(f"\nCorrupt / Skipped Files  : {skipped_count}")
    print("=" * 76 + "\n", flush=True)

    return summary


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="VocalSet Importer & Manifest Generator")
    parser.add_argument("--vocalset-dir", default=os.path.join(PROJECT_ROOT, "Data", "VocalSet"), help="Path to extracted VocalSet")
    parser.add_argument("--manifest", default=os.path.join(PROJECT_ROOT, "Assets", "training_data_manifest.json"), help="Manifest JSON path")
    args = parser.parse_args()

    import_vocalset(args.vocalset_dir, args.manifest)
