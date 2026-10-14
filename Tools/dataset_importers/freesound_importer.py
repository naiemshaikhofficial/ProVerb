#!/usr/bin/env python3
"""
Freesound Commercial License-Filtered Importer & Auditor
ProducerToy — ProVerb Smart Algorithmic Reverb

Official Source: https://freesound.org/
API Documentation: https://freesound.org/docs/api/terms_of_use.html

COMPLIANCE & LEGAL FILTER RULES:
1. Whitelist for Commercial Model Training:
   - Creative Commons 0 (CC0 1.0 Universal / Public Domain)
   - Creative Commons Attribution (CC BY 4.0, CC BY 3.0, CC BY 2.0) with required attribution.
2. Strict Exclusion / Quarantine:
   - ALL Non-Commercial licenses (CC BY-NC 3.0, CC BY-NC 4.0, CC BY-NC-SA, etc.)
   - Any recording with missing, unverifiable, or ambiguous licensing terms.
   - Any recording flagging negative AI/ML training preferences.
3. No unrestricted bulk scraping without explicit commercial authorization under Freesound API Terms.
"""

import os
import sys
import json
from typing import Dict, List, Optional, Tuple

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from manifest_manager import DatasetManifest

DATASET_NAME = "Freesound"
SOURCE_URL = "https://freesound.org/"

# License evaluation lookup
COMMERCIAL_WHITELIST = {
    "http://creativecommons.org/publicdomain/zero/1.0/": ("CC0-1.0", "COMMERCIALLY_PERMITTED"),
    "https://creativecommons.org/publicdomain/zero/1.0/": ("CC0-1.0", "COMMERCIALLY_PERMITTED"),
    "cc0": ("CC0-1.0", "COMMERCIALLY_PERMITTED"),
    "http://creativecommons.org/licenses/by/3.0/": ("CC-BY-3.0", "ATTRIBUTION_REQUIRED"),
    "http://creativecommons.org/licenses/by/4.0/": ("CC-BY-4.0", "ATTRIBUTION_REQUIRED"),
    "https://creativecommons.org/licenses/by/4.0/": ("CC-BY-4.0", "ATTRIBUTION_REQUIRED"),
    "cc-by": ("CC-BY-4.0", "ATTRIBUTION_REQUIRED"),
}

EXCLUDED_LICENSES = [
    "noncommercial", "by-nc", "nc", "sampling+", "sampling plus"
]


def evaluate_sound_license(license_str: str, ai_opt_out: bool = False) -> Tuple[bool, str, str, str]:
    """
    Evaluates whether a Freesound audio item is legally eligible for commercial ML training.
    Returns: (is_eligible, standardized_license, permission_status, reason)
    """
    if ai_opt_out:
        return False, "UNKNOWN", "NON_COMMERCIAL_RESTRICTED", "Uploader/metadata opted out of AI model training."

    lic_lower = license_str.lower().strip()

    # Check explicit non-commercial restrictions
    for excluded in EXCLUDED_LICENSES:
        if excluded in lic_lower:
            return False, license_str, "NON_COMMERCIAL_RESTRICTED", f"Contains non-commercial restriction: '{excluded}'"

    # Check commercial whitelist
    for pattern, (std_lic, perm_status) in COMMERCIAL_WHITELIST.items():
        if pattern in lic_lower or pattern == lic_lower:
            return True, std_lic, perm_status, "Eligible under verified open commercial license."

    return False, license_str, "AMBIGUOUS_QUARANTINED", "License not recognized in verified commercial whitelist."


def import_freesound_manifest(manifest_json_path: str, dataset_manifest: DatasetManifest) -> Dict:
    """
    Parses a user-supplied Freesound collection manifest, auditing and importing eligible items.
    """
    print(f"[*] Auditing Freesound collection manifest: {manifest_json_path}...", flush=True)

    with open(manifest_json_path, "r", encoding="utf-8") as f:
        items = json.load(f)

    if isinstance(items, dict) and "sounds" in items:
        items = items["sounds"]

    included_count = 0
    excluded_count = 0
    exclusions_log = []

    for item in items:
        sound_id = str(item.get("id", item.get("sound_id", "unknown")))
        title = item.get("name", item.get("title", f"Sound_{sound_id}"))
        fpath = item.get("file_path", item.get("path", ""))
        license_str = item.get("license", "unknown")
        username = item.get("username", item.get("uploader", "Unknown Uploader"))
        target_class = item.get("target_class", 6)
        ai_opt_out = bool(item.get("ai_opt_out", False))

        is_eligible, std_lic, perm_status, reason = evaluate_sound_license(license_str, ai_opt_out)

        if not is_eligible:
            excluded_count += 1
            exclusions_log.append({
                "sound_id": sound_id,
                "title": title,
                "license": license_str,
                "reason": reason
            })
            continue

        # Add to manifest
        attribution = f"Sound #{sound_id} '{title}' by {username} via Freesound ({std_lic})."
        dataset_manifest.add_recording(
            file_path=fpath,
            dataset_name=DATASET_NAME,
            source_track_id=f"FS_{sound_id}",
            recording_id=f"FS_{sound_id}",
            class_index=int(target_class),
            class_mapping_rationale=item.get("rationale", "User labeled instrument audio"),
            license_id=std_lic,
            attribution=attribution,
            permission_status=perm_status,
            sample_rate=float(item.get("sample_rate", 44100)),
            duration_seconds=float(item.get("duration", 2.5)),
            channels=int(item.get("channels", 1)),
            creator=username,
            ai_training_allowed=True,
            split="unassigned"
        )
        included_count += 1

    summary = {
        "total_audited": len(items),
        "commercially_included": included_count,
        "excluded_quarantined": excluded_count,
        "sample_exclusions": exclusions_log[:5]
    }

    print(f"\n[✓] Freesound Audit Completed:")
    print(f"    - Commercially Included: {included_count}")
    print(f"    - Excluded / Quarantined: {excluded_count}\n")
    return summary


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python freesound_importer.py <input_freesound_manifest.json> [output_manifest.json]")
        sys.exit(1)

    in_manifest = sys.argv[1]
    out_manifest = sys.argv[2] if len(sys.argv) > 2 else "Assets/training_data_manifest.json"

    mf = DatasetManifest()
    import_freesound_manifest(in_manifest, mf)
    mf.save_json(out_manifest)
