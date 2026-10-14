#!/usr/bin/env python3
"""
ProVerb Training Data Manifest Manager
ProducerToy — ProVerb Smart Algorithmic Reverb

Manages, validates, and serializes the structured training dataset manifest:
Assets/training_data_manifest.json

Ensures strict license provenance, source recording tracking (GroupKFold),
and commercial use eligibility tracking.
"""

import os
import sys
import json
import hashlib
from typing import Dict, List, Optional, Any

SCHEMA_VERSION = "1.0.0"

# Established ProVerb class contract
CLASS_NAMES = [
    "Drums",
    "Vocals",
    "Guitar",
    "Piano / Keys",
    "Bass",
    "Synth / Pad",
    "Other / Uncertain"
]

# Supported legal permission statuses
PERMISSION_STATUSES = [
    "COMMERCIALLY_PERMITTED",        # e.g., CC0, CC-BY with attribution
    "ATTRIBUTION_REQUIRED",          # e.g., CC-BY-4.0
    "NON_COMMERCIAL_RESTRICTED",     # e.g., CC-BY-NC, MUSDB18 research terms
    "AMBIGUOUS_QUARANTINED"          # missing/unverified terms, excluded from commercial training
]


def compute_file_sha256(filepath: str, max_bytes: int = 10 * 1024 * 1024) -> str:
    """Computes SHA256 hash of a file (up to max_bytes for speed)."""
    hasher = hashlib.sha256()
    try:
        with open(filepath, "rb") as f:
            chunk = f.read(max_bytes)
            hasher.update(chunk)
        return hasher.hexdigest()
    except Exception as e:
        return f"error:{e}"


class DatasetManifest:
    def __init__(self, description: str = "ProVerb Training Audio Manifest"):
        self.metadata = {
            "schema_version": SCHEMA_VERSION,
            "description": description,
            "classes": {idx: name for idx, name in enumerate(CLASS_NAMES)},
            "datasets": {}
        }
        self.records: List[Dict[str, Any]] = []

    def register_dataset_source(self, dataset_name: str, version: str, source_url: str,
                                license_id: str, attribution: str, default_permission: str):
        self.metadata["datasets"][dataset_name] = {
            "version": version,
            "source_url": source_url,
            "license": license_id,
            "attribution": attribution,
            "permission_status": default_permission
        }

    def add_recording(self,
                      file_path: str,
                      dataset_name: str,
                      source_track_id: str,
                      recording_id: str,
                      class_index: int,
                      class_mapping_rationale: str,
                      license_id: str,
                      attribution: str,
                      permission_status: str,
                      sample_rate: float,
                      duration_seconds: float,
                      channels: int,
                      creator: Optional[str] = None,
                      ai_training_allowed: bool = True,
                      split: str = "unassigned") -> Dict[str, Any]:
        """
        Adds a source recording entry to the manifest.
        """
        assert 0 <= class_index < len(CLASS_NAMES), f"Invalid class index {class_index}"
        assert permission_status in PERMISSION_STATUSES, f"Invalid permission status {permission_status}"
        assert split in ["train", "val", "test", "unassigned", "excluded"]

        target_path = file_path
        if not os.path.exists(target_path):
            tools_dir = os.path.dirname(os.path.abspath(__file__))
            p_cand = os.path.normpath(os.path.join(tools_dir, "..", file_path))
            if os.path.exists(p_cand):
                target_path = p_cand

        sha256 = compute_file_sha256(target_path) if os.path.exists(target_path) else "pending"

        record = {
            "recording_id": recording_id,
            "parent_track_id": source_track_id,
            "dataset_name": dataset_name,
            "file_path": os.path.normpath(file_path),
            "file_sha256": sha256,
            "sample_rate": float(sample_rate),
            "channels": int(channels),
            "duration_seconds": float(duration_seconds),
            "class_index": int(class_index),
            "class_name": CLASS_NAMES[class_index],
            "class_mapping_rationale": class_mapping_rationale,
            "license": license_id,
            "attribution": attribution,
            "creator": creator or "Unknown / Specified in dataset metadata",
            "permission_status": permission_status,
            "ai_training_allowed": bool(ai_training_allowed),
            "split": split
        }
        self.records.append(record)
        return record

    def save_json(self, output_path: str):
        """Serializes manifest to JSON."""
        os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
        data = {
            "metadata": self.metadata,
            "total_recordings": len(self.records),
            "recordings": self.records
        }
        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
        print(f"[*] Manifest saved ({len(self.records)} records) -> {output_path}", flush=True)

    @classmethod
    def load_json(cls, input_path: str) -> "DatasetManifest":
        with open(input_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        manifest = cls(description=data["metadata"].get("description", ""))
        manifest.metadata = data["metadata"]
        manifest.records = data.get("recordings", [])
        return manifest
