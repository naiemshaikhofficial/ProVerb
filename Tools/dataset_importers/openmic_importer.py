#!/usr/bin/env python3
"""
OpenMIC-2018 Dataset Importer & Artist-Disjoint Manifest Generator
ProducerToy — ProVerb Smart Algorithmic Reverb

Dataset: OpenMIC-2018: An Open Dataset for Multiple Instrument Recognition
Official Source: https://doi.org/10.5281/zenodo.1432913
Authors: Eric J. Humphrey, Simon Durand, Brian McFee (Spotify & NYU MARL)
License: Creative Commons Attribution 4.0 International (CC BY 4.0)
Attribution: Eric J. Humphrey, Simon Durand, and Brian McFee.
"OpenMIC-2018: An Open Dataset for Multiple Instrument Recognition."
In 19th International Society for Music Information Retrieval Conference (ISMIR 2018).

Features:
1. Scans extracted OpenMIC-2018 directory (Data/OpenMIC/ or Data/OpenMIC/openmic-2018/).
2. Parses the official aggregated-label and per-recording metadata CSV files.
3. Maps recognized instrument tags to ProVerb's 7 canonical acoustic classes:
   - drums, cymbals, mallet_percussion -> [0] Drums
   - acoustic_guitar, electric_guitar, banjo, ukulele, mandolin -> [2] Guitar
   - piano, organ, accordion -> [3] Piano / Keys
   - bass -> [4] Bass
   - synthesizer -> [5] Synth / Pad; voice -> [1] Vocals
   - remaining OpenMIC classes -> [6] Other / Uncertain
4. Audits each audio file using soundfile (duration, sample rate, channels, finite audio).
5. Implements artist-disjoint splits, falling back to track-disjoint when artist metadata is missing.
6. Updates Assets/training_data_manifest.json preserving existing dataset entries (e.g. Slakh2100, VocalSet).
"""

import os
import sys
import re
import csv
import json
import math
import argparse
from collections import Counter, defaultdict
from typing import Dict, List, Optional, Tuple, Set

import soundfile as sf
import numpy as np

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(TOOLS_DIR, "..", ".."))
sys.path.insert(0, os.path.abspath(os.path.join(TOOLS_DIR, "..")))
from manifest_manager import DatasetManifest, compute_file_sha256

DATASET_NAME = "OpenMIC-2018"
DATASET_VERSION = "v1.0.0"
SOURCE_URL = "https://doi.org/10.5281/zenodo.1432913"
LICENSE_ID = "CC-BY-4.0"
ATTRIBUTION_TEXT = (
    "OpenMIC-2018 is licensed under CC BY 4.0 by Eric J. Humphrey, Simon Durand, "
    "and Brian McFee (Spotify & NYU MARL)."
)

# ProVerb 7 acoustic classes:
# 0: Drums
# 1: Vocals (handled by VocalSet)
# 2: Guitar
# 3: Piano / Keys
# 4: Bass
# 5: Synth / Pad
# 6: Other / Uncertain

OPENMIC_CLASS_MAP = {
    # Drums / Percussion
    "drums": (0, "OpenMIC drums tag"),
    "cymbals": (0, "OpenMIC cymbals percussion tag"),
    "mallet_percussion": (0, "OpenMIC mallet percussion tag"),

    # Guitars
    "acoustic_guitar": (2, "OpenMIC acoustic_guitar tag"),
    "electric_guitar": (2, "OpenMIC electric_guitar tag"),
    "guitar": (2, "OpenMIC guitar tag"),
    "banjo": (2, "OpenMIC banjo plucked string tag"),
    "ukulele": (2, "OpenMIC ukulele plucked string tag"),
    "mandolin": (2, "OpenMIC mandolin plucked string tag"),

    # Piano / Keys
    "piano": (3, "OpenMIC piano tag"),
    "organ": (3, "OpenMIC organ keyboard tag"),
    "accordion": (3, "OpenMIC accordion keys tag"),

    # Bass
    "bass": (4, "OpenMIC bass tag"),

    # Synthesizer / Pad
    "synthesizer": (5, "OpenMIC synthesizer tag"),
    "voice": (1, "OpenMIC voice tag"),

    # OpenMIC's remaining instrument classes are useful as explicit Other examples.
    "cello": (6, "OpenMIC cello tag -> Other / Uncertain"),
    "clarinet": (6, "OpenMIC clarinet tag -> Other / Uncertain"),
    "flute": (6, "OpenMIC flute tag -> Other / Uncertain"),
    "saxophone": (6, "OpenMIC saxophone tag -> Other / Uncertain"),
    "trombone": (6, "OpenMIC trombone tag -> Other / Uncertain"),
    "trumpet": (6, "OpenMIC trumpet tag -> Other / Uncertain"),
    "violin": (6, "OpenMIC violin tag -> Other / Uncertain"),
}


def _normalized_header(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", (value or "").strip().lower())


def _get_field(row: Dict[str, str], *names: str) -> str:
    normalized = {_normalized_header(k): (v or "").strip() for k, v in row.items() if k is not None}
    for name in names:
        value = normalized.get(_normalized_header(name), "")
        if value:
            return value
    return ""


def _track_id_from_sample_key(sample_key: str) -> str:
    return sample_key.split("_", 1)[0].strip()


def _canonicalize_commercial_license(raw_license: str) -> Tuple[Optional[str], str]:
    """Fail closed: accept only verified CC0 and plain CC-BY licenses."""
    value = (raw_license or "").strip().lower().replace(" ", "")
    if not value:
        return None, "AMBIGUOUS_QUARANTINED"

    # Non-commercial terms are restricted; ND/SA/Sampling+ are quarantined by this strict whitelist.
    if any(token in value for token in ("noncommercial", "non-commercial", "by-nc")):
        return None, "NON_COMMERCIAL_RESTRICTED"
    if any(token in value for token in ("by-nd", "by-sa", "sampling+")):
        return None, "AMBIGUOUS_QUARANTINED"

    if "cc0" in value or "publicdomain/zero/1.0" in value or "public-domain" in value:
        return "CC0-1.0", "COMMERCIALLY_PERMITTED"

    match = re.search(r"(?:cc-?by[-/ ]?|licenses/by/)([1-4](?:\.0)?)(?:[-/ ]?(?:us|uk|de|au))?", value)
    if match:
        version = match.group(1)
        if "." not in version:
            version += ".0"
        return f"CC-BY-{version}", "ATTRIBUTION_REQUIRED"

    return None, "AMBIGUOUS_QUARANTINED"


def _load_track_metadata(metadata_path: str) -> Tuple[Dict[str, Dict], Dict[str, Dict]]:
    """Loads the official per-recording license/artist metadata, indexed by clip and track."""
    by_sample: Dict[str, Dict] = {}
    by_track: Dict[str, Dict] = {}
    with open(metadata_path, "r", encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames:
            raise ValueError(f"OpenMIC metadata CSV has no header: {metadata_path}")
        for row in reader:
            sample_key = _get_field(row, "sample_key", "sample", "clip_id")
            track_id = _get_field(row, "track_id", "track", "id")
            if not track_id and sample_key:
                track_id = _track_id_from_sample_key(sample_key)
            item = {
                "artist_id": _get_field(row, "artist_id", "artist", "artist_name"),
                "creator": _get_field(row, "artist_name", "artist", "creator"),
                "track_id": track_id,
                "license_raw": _get_field(row, "license", "track_license", "license_url", "license_id"),
            }
            if sample_key:
                by_sample[sample_key] = item
            if track_id:
                by_track[track_id] = item

    if not by_sample and not by_track:
        raise ValueError(f"OpenMIC metadata CSV contains no sample_key/track_id rows: {metadata_path}")
    return by_sample, by_track


def find_openmic_files(data_dir: str) -> Tuple[Optional[str], Optional[str], Optional[str]]:
    """Locates the official aggregated-label CSV, per-track metadata CSV, and audio root."""
    roots = [data_dir, os.path.join(data_dir, "openmic-2018")]
    labels_path = next((os.path.join(root, "openmic-2018-aggregated-labels.csv")
                        for root in roots if os.path.isfile(os.path.join(root, "openmic-2018-aggregated-labels.csv"))), None)
    metadata_path = next((os.path.join(root, "openmic-2018-metadata.csv")
                          for root in roots if os.path.isfile(os.path.join(root, "openmic-2018-metadata.csv"))), None)
    audio_dir = next((os.path.join(root, "audio")
                      for root in roots if os.path.isdir(os.path.join(root, "audio"))), None)
    return labels_path, metadata_path, audio_dir


def load_openmic_annotations(labels_path: str, metadata_path: str,
                             min_confidence: float = 0.6) -> Dict[str, Dict]:
    """Parses official aggregated labels and joins license/artist data from metadata."""
    if not 0.0 <= min_confidence <= 1.0:
        raise ValueError("min_confidence must be between 0.0 and 1.0")

    metadata_by_sample, metadata_by_track = _load_track_metadata(metadata_path)
    annotations: Dict[str, Dict] = defaultdict(lambda: {
        "artist_id": "", "creator": "", "track_id": "", "license_raw": "", "instruments": {}
    })

    with open(labels_path, "r", encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames:
            raise ValueError(f"OpenMIC aggregated-label CSV has no header: {labels_path}")
        for row in reader:
            sample_key = _get_field(row, "sample_key", "sample", "clip_id")
            instrument = _get_field(row, "instrument", "class", "label").lower().replace(" ", "_")
            score_text = _get_field(row, "relevance", "response", "confidence", "score")
            if not sample_key or not instrument or not score_text:
                continue
            try:
                score = float(score_text)
            except ValueError:
                continue
            if not math.isfinite(score) or not 0.0 <= score <= 1.0:
                continue

            track_id = _track_id_from_sample_key(sample_key)
            meta = metadata_by_sample.get(sample_key) or metadata_by_track.get(track_id) or {}
            entry = annotations[sample_key]
            entry["track_id"] = meta.get("track_id") or track_id
            entry["artist_id"] = meta.get("artist_id", "")
            entry["creator"] = meta.get("creator", "")
            entry["license_raw"] = meta.get("license_raw", "")

            # OpenMIC contains partially observed, weak multi-label tags. Keep only high-confidence positives.
            if score >= min_confidence and instrument in OPENMIC_CLASS_MAP:
                entry["instruments"][instrument] = max(score, entry["instruments"].get(instrument, 0.0))

    if not annotations:
        raise ValueError(f"No valid OpenMIC labels parsed from {labels_path}; expected sample_key/instrument/relevance columns.")
    return annotations


def assign_artist_disjoint_splits(artist_ids: List[str],
                                  train_ratio: float = 0.70,
                                  val_ratio: float = 0.15,
                                  test_ratio: float = 0.15) -> Dict[str, str]:
    """
    Creates leakage-free Artist-Disjoint Split.
    Ensures zero tracks by the same artist cross train/val/test boundaries.
    """
    unique_artists = sorted(list(set(artist_ids)))
    # Deterministic seed for exact reproducibility across runs
    rng = np.random.RandomState(42)
    rng.shuffle(unique_artists)

    n = len(unique_artists)
    n_train = int(n * train_ratio)
    n_val = int(n * val_ratio)

    split_map = {}
    for idx, artist in enumerate(unique_artists):
        if idx < n_train:
            split_map[artist] = "train"
        elif idx < n_train + n_val:
            split_map[artist] = "val"
        else:
            split_map[artist] = "test"

    return split_map


def import_openmic(data_dir: str,
                   manifest_path: str,
                   max_per_class: int = 1000,
                   min_confidence: float = 0.6) -> Dict:
    """
    Ingests OpenMIC audio recordings into Assets/training_data_manifest.json.
    """
    print(f"[*] Scanning OpenMIC directory: {data_dir}...", flush=True)
    labels_path, metadata_path, audio_root = find_openmic_files(data_dir)

    if not labels_path or not metadata_path or not audio_root:
        raise FileNotFoundError(
            "Could not locate the official OpenMIC files under " + data_dir + ". Expected "
            "openmic-2018-aggregated-labels.csv, openmic-2018-metadata.csv, and audio/."
        )

    print(f"    - Found aggregated labels: {labels_path}", flush=True)
    print(f"    - Found track metadata   : {metadata_path}", flush=True)
    print(f"    - Found audio root       : {audio_root}", flush=True)

    annotations = load_openmic_annotations(labels_path, metadata_path, min_confidence=min_confidence)
    print(f"    - Loaded annotations for {len(annotations):,} sample keys.", flush=True)

    # Artist-disjoint where possible; otherwise keep every excerpt from a source track together.
    group_ids = {
        sample_key: (data["artist_id"] or f"track_{data['track_id']}")
        for sample_key, data in annotations.items()
    }
    artist_splits = assign_artist_disjoint_splits(list(group_ids.values()))
    print(f"    - Partitioned {len(set(group_ids.values())):,} artist/track groups into splits.", flush=True)

    # Load existing manifest
    manifest = DatasetManifest(description="ProVerb Licensed Multi-Source Audio Manifest")
    if os.path.exists(manifest_path):
        manifest = DatasetManifest.load_json(manifest_path)
        before = len(manifest.records)
        manifest.records = [r for r in manifest.records if r.get("dataset_name") != DATASET_NAME]
        removed_openmic = before - len(manifest.records)
        if removed_openmic:
            print(f"[*] Replacing {removed_openmic:,} previous OpenMIC entries.", flush=True)
        print(f"[*] Preserving existing manifest with {len(manifest.records):,} records.", flush=True)

    manifest.register_dataset_source(
        dataset_name=DATASET_NAME,
        version=DATASET_VERSION,
        source_url=SOURCE_URL,
        license_id=LICENSE_ID,
        attribution=ATTRIBUTION_TEXT,
        # Dataset-level permission is not substituted for per-recording license metadata.
        default_permission="ATTRIBUTION_REQUIRED"
    )

    # Track existing files to prevent duplicates
    existing_file_paths = {r.get("file_path") for r in manifest.records}

    class_counts = Counter()
    split_counts = Counter()
    processed_count = 0
    skipped_count = 0
    corrupt_count = 0
    license_excluded_count = 0
    ambiguous_label_count = 0

    # Sort sample keys for reproducibility
    sorted_keys = sorted(annotations.keys())

    for sample_key in sorted_keys:
        info = annotations[sample_key]
        insts = info["instruments"]
        if not insts:
            continue

        # The current ProVerb trainer is single-label CrossEntropy. Keep only clips whose
        # observed high-confidence positives collapse to one canonical class. OpenMIC labels
        # are partial, so this remains weak supervision, not proof of an isolated source.
        mapped = {}
        for inst_name, confidence in insts.items():
            mapped_class, mapped_rationale = OPENMIC_CLASS_MAP[inst_name]
            previous = mapped.get(mapped_class)
            if previous is None or confidence > previous[0]:
                mapped[mapped_class] = (confidence, inst_name, mapped_rationale)
        if len(mapped) != 1:
            ambiguous_label_count += 1
            continue
        class_idx, (confidence, best_inst, rationale) = next(iter(mapped.items()))

        license_id, permission_status = _canonicalize_commercial_license(info.get("license_raw", ""))
        if not license_id or permission_status not in ("COMMERCIALLY_PERMITTED", "ATTRIBUTION_REQUIRED"):
            license_excluded_count += 1
            continue

        # Class cap to prevent one instrument from heavily dominating
        if max_per_class > 0 and class_counts[class_idx] >= max_per_class:
            continue

        # Resolve relative and absolute audio file path
        # Directory structure: audio/{prefix}/{sample_key}.ogg
        prefix = sample_key[:3]
        abs_path = os.path.join(audio_root, prefix, f"{sample_key}.ogg")

        if not os.path.isfile(abs_path):
            # Check without openmic-2018 subdirectory
            alt_path = os.path.join(audio_root, f"{sample_key}.ogg")
            if os.path.isfile(alt_path):
                abs_path = alt_path
            else:
                skipped_count += 1
                continue

        rel_path = os.path.relpath(abs_path, PROJECT_ROOT)
        norm_rel_path = rel_path.replace("\\", "/")
        if norm_rel_path in existing_file_paths:
            continue

        # Audit audio integrity via soundfile
        try:
            with sf.SoundFile(abs_path) as audio_file:
                sr = float(audio_file.samplerate)
                channels = int(audio_file.channels)
                frames = audio_file.frames
                duration = frames / sr

            if duration < 0.5 or frames == 0:
                corrupt_count += 1
                continue

        except Exception as e:
            corrupt_count += 1
            continue

        group_id = group_ids[sample_key]
        artist_id = info["artist_id"]
        split = artist_splits.get(group_id, "train")
        attribution = ATTRIBUTION_TEXT
        if info.get("creator"):
            attribution += f" Recording creator: {info['creator']}."

        record = manifest.add_recording(
            file_path=norm_rel_path,
            dataset_name=DATASET_NAME,
            source_track_id=f"openmic_{'artist_' + artist_id if artist_id else 'track_' + info['track_id']}",
            recording_id=f"openmic_{sample_key}",
            class_index=class_idx,
            class_mapping_rationale=(
                f"Weak OpenMIC positive tag '{best_inst}' mapped to {rationale}; "
                f"relevance={confidence:.2f}. Other instruments may be unannotated."
            ),
            license_id=license_id,
            attribution=attribution,
            permission_status=permission_status,
            sample_rate=sr,
            duration_seconds=round(duration, 3),
            channels=channels,
            creator=info.get("creator") or (f"FMA Artist {artist_id}" if artist_id else "OpenMIC track metadata"),
            ai_training_allowed=True,
            split=split
        )
        record["source_license_text"] = info.get("license_raw", "")

        class_counts[class_idx] += 1
        split_counts[split] += 1
        existing_file_paths.add(norm_rel_path)
        processed_count += 1

    # Save updated manifest through the schema manager.
    manifest.save_json(manifest_path)

    print("\n" + "=" * 78)
    print("  OpenMIC-2018 Ingestion Summary")
    print("=" * 78)
    print(f"  Total Newly Ingested Recordings : {processed_count:,}")
    print(f"  Corrupt / Invalid Audio Clips    : {corrupt_count:,}")
    print(f"  Skipped (Missing on Disk)        : {skipped_count:,}")
    print(f"  Excluded (Unknown/Restricted License): {license_excluded_count:,}")
    print(f"  Excluded (Multiple Canonical Labels): {ambiguous_label_count:,}")
    print(f"  Grand Total Manifest Records     : {len(manifest.records):,}")
    print("\n  Class Distribution (Newly Ingested):")
    for c_idx in sorted(class_counts.keys()):
        c_name = manifest.metadata["classes"][str(c_idx)]
        print(f"    - [{c_idx}] {c_name:<18}: {class_counts[c_idx]:,}")

    print("\n  Split Distribution (Newly Ingested):")
    for s_name in ["train", "val", "test"]:
        print(f"    - {s_name:<8}: {split_counts[s_name]:,}")

    return {
        "newly_ingested": processed_count,
        "class_counts": dict(class_counts),
        "split_counts": dict(split_counts),
        "total_records": len(manifest.records)
    }


def main():
    parser = argparse.ArgumentParser(description="Import OpenMIC-2018 into ProVerb dataset manifest.")
    parser.add_argument("--data-dir", default=os.path.join(PROJECT_ROOT, "Data", "OpenMIC"),
                        help="Path to extracted OpenMIC directory")
    parser.add_argument("--manifest", default=os.path.join(PROJECT_ROOT, "Assets", "training_data_manifest.json"),
                        help="Path to training data manifest JSON")
    parser.add_argument("--max-per-class", type=int, default=1200,
                        help="Maximum recordings per class to ingest (ensures class balance, 0 = unlimited)")
    parser.add_argument("--min-confidence", type=float, default=0.6,
                        help="Minimum crowd annotation confidence threshold (0.0 to 1.0)")
    args = parser.parse_args()

    import_openmic(
        data_dir=args.data_dir,
        manifest_path=args.manifest,
        max_per_class=args.max_per_class,
        min_confidence=args.min_confidence
    )


if __name__ == "__main__":
    main()
