#!/usr/bin/env python3
"""Add GuitarSet and SYN-SE1 with grouped provenance to ProVerb's manifest."""

import argparse
import json
import os
import re
import soundfile as sf

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(TOOLS_DIR, "..", ".."))
import sys
sys.path.insert(0, os.path.dirname(TOOLS_DIR))
from manifest_manager import DatasetManifest

GUITARSET = {
    "name": "GuitarSet",
    "version": "v1.1.0",
    "source_url": "https://doi.org/10.5281/zenodo.3371780",
    "license": "CC-BY-4.0",
    "attribution": (
        'GuitarSet: Qingyang Xi, Rachel M. Bittner, Johan Pauwels, Xuzhou Ye, and '
        'Juan P. Bello, "GuitarSet: A Dataset for Guitar Transcription," ISMIR 2018. '
        "Licensed under CC BY 4.0."
    ),
}
SYNSE1 = {
    "name": "WaivOps-SYN-SE1",
    "version": "v1.0",
    "source_url": "https://doi.org/10.5281/zenodo.13993751",
    "license": "CC-BY-4.0",
    "attribution": (
        "WaivOps SYN-SE1, Patchbanks, licensed under CC BY 4.0; "
        "https://doi.org/10.5281/zenodo.13993751."
    ),
}


def _relative(path):
    return os.path.relpath(path, PROJECT_ROOT).replace("\\", "/")


def _audio_info(path):
    with sf.SoundFile(path) as audio:
        if audio.frames <= 0 or audio.samplerate <= 0:
            raise ValueError("empty or invalid audio")
        return float(audio.samplerate), int(audio.channels), round(audio.frames / audio.samplerate, 6)


def _guitarset_records(manifest, data_dir):
    candidates = [p for p in _walk_wavs(data_dir) if "mono-pickup" in p.replace("\\", "/").lower() or "pickup_mix" in p.replace("\\", "/").lower()]
    if not candidates:
        candidates = _walk_wavs(data_dir)
    if not candidates:
        raise FileNotFoundError(f"No GuitarSet WAV files found under {data_dir}")
    manifest.register_dataset_source(
        GUITARSET["name"], GUITARSET["version"], GUITARSET["source_url"],
        GUITARSET["license"], GUITARSET["attribution"], "ATTRIBUTION_REQUIRED"
    )
    added = 0
    for path in sorted(candidates):
        base = os.path.basename(path)
        match = re.match(r"^(\d{2})_", base)
        if not match:
            continue
        player = match.group(1)
        try:
            sample_rate, channels, duration = _audio_info(path)
        except Exception as exc:
            print(f"[skip] unreadable GuitarSet file {path}: {exc}")
            continue
        manifest.add_recording(
            file_path=_relative(path), dataset_name=GUITARSET["name"],
            source_track_id=f"GuitarSet_player_{player}",
            recording_id=f"GuitarSet_{os.path.splitext(base)[0]}",
            class_index=2,
            class_mapping_rationale=(
                "GuitarSet mono mixed pickup performance recorded from the acoustic guitar; "
                "the source supplies isolated guitar pickup audio and aligned annotations."
            ),
            license_id=GUITARSET["license"], attribution=GUITARSET["attribution"],
            permission_status="ATTRIBUTION_REQUIRED", sample_rate=sample_rate,
            duration_seconds=duration, channels=channels,
            creator="GuitarSet performers / NYU MARL and Queen Mary University of London",
            ai_training_allowed=True, split="unassigned"
        )
        added += 1
    if not added:
        raise RuntimeError(f"Found WAVs but none matched GuitarSet player filename convention under {data_dir}")
    return added


def _walk_wavs(root):
    return [os.path.join(folder, filename)
            for folder, _, files in os.walk(root)
            for filename in files if filename.lower().endswith(".wav")]


def _synse1_records(manifest, data_dir):
    metadata_files = []
    for folder, _, files in os.walk(data_dir):
        metadata_files.extend(os.path.join(folder, f) for f in files
                              if f.lower() == "syn_se1_id_001.json")
    if len(metadata_files) != 1:
        raise FileNotFoundError(
            f"Expected one syn_se1_id_001.json under {data_dir}; found {len(metadata_files)}"
        )
    metadata_path = metadata_files[0]
    with open(metadata_path, "r", encoding="utf-8") as stream:
        metadata = json.load(stream)
    audio_root = os.path.join(os.path.dirname(metadata_path), metadata.get("directory", ""))
    if not os.path.isdir(audio_root):
        # Some archive versions place the WAV directory beside the JSON file.
        audio_root = os.path.dirname(metadata_path)

    manifest.register_dataset_source(
        SYNSE1["name"], SYNSE1["version"], SYNSE1["source_url"], SYNSE1["license"],
        SYNSE1["attribution"], "ATTRIBUTION_REQUIRED"
    )
    added = 0
    for item in metadata.get("files", []):
        filename = os.path.basename(str(item.get("file", "")))
        path = os.path.join(audio_root, filename)
        if not filename or not os.path.isfile(path):
            print(f"[skip] missing SYN-SE1 sample {filename}")
            continue
        preset = str(item.get("preset", "unknown"))
        try:
            sample_rate, channels, duration = _audio_info(path)
        except Exception as exc:
            print(f"[skip] unreadable SYN-SE1 file {path}: {exc}")
            continue
        manifest.add_recording(
            file_path=_relative(path), dataset_name=SYNSE1["name"],
            source_track_id=f"SYN-SE1_preset_{preset}",
            recording_id=f"SYN-SE1_{os.path.splitext(filename)[0]}",
            class_index=5,
            class_mapping_rationale=(
                "SYN-SE1 recording of an analog synthesizer preset; note key is "
                f"{item.get('note', 'unspecified')}. Synth-bass/lead coverage; not a pad-specific label."
            ),
            license_id=SYNSE1["license"], attribution=SYNSE1["attribution"],
            permission_status="ATTRIBUTION_REQUIRED", sample_rate=sample_rate,
            duration_seconds=duration, channels=channels,
            creator="Patchbanks / WaivOps; Studio Electronics SE1",
            ai_training_allowed=True, split="unassigned"
        )
        added += 1
    if not added:
        raise RuntimeError(f"No SYN-SE1 recordings imported from {metadata_path}")
    return added


def import_datasets(guitarset_dir, synse1_dir, manifest_path):
    if os.path.exists(manifest_path):
        manifest = DatasetManifest.load_json(manifest_path)
    else:
        manifest = DatasetManifest()
    replace_names = {GUITARSET["name"], SYNSE1["name"]}
    before = len(manifest.records)
    manifest.records = [r for r in manifest.records if r.get("dataset_name") not in replace_names]
    replaced = before - len(manifest.records)
    guitar_count = _guitarset_records(manifest, guitarset_dir)
    synth_count = _synse1_records(manifest, synse1_dir)
    manifest.save_json(manifest_path)
    print(f"Replaced prior records: {replaced}")
    print(f"GuitarSet guitar recordings: {guitar_count}")
    print(f"SYN-SE1 synth recordings: {synth_count}")
    print(f"Manifest total recordings: {len(manifest.records)}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--guitarset-dir", default=os.path.join(PROJECT_ROOT, "Data", "GuitarSet"))
    parser.add_argument("--synse1-dir", default=os.path.join(PROJECT_ROOT, "Data", "SYN-SE1"))
    parser.add_argument("--manifest", default=os.path.join(PROJECT_ROOT, "Assets", "training_data_manifest.json"))
    args = parser.parse_args()
    import_datasets(args.guitarset_dir, args.synse1_dir, args.manifest)


if __name__ == "__main__":
    main()
