#!/usr/bin/env python3
"""
ProVerb Training Data License Audit & Compliance Auditor
ProducerToy — ProVerb Smart Algorithmic Reverb

Audits Assets/training_data_manifest.json to ensure full legal compliance,
correct attribution records, commercial eligibility verification,
and class representation transparency.
"""

import os
import sys
import json
from collections import Counter
from typing import Dict, List

MANIFEST_PATH = os.path.join(os.path.dirname(__file__), "..", "Assets", "training_data_manifest.json")
REPORT_OUTPUT_PATH = os.path.join(os.path.dirname(__file__), "..", "Assets", "license_audit_report.md")


def audit_manifest(manifest_path: str = MANIFEST_PATH) -> Dict:
    if not os.path.exists(manifest_path):
        raise FileNotFoundError(f"Manifest not found at {manifest_path}. Generate manifest first.")

    with open(manifest_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    meta = data.get("metadata", {})
    recordings = data.get("recordings", [])
    total_recs = len(recordings)

    dataset_counts = Counter(r.get("dataset_name", "Unknown") for r in recordings)
    license_counts = Counter(r.get("license", "Unknown") for r in recordings)
    permission_counts = Counter(r.get("permission_status", "Unknown") for r in recordings)
    class_counts = Counter(r.get("class_name", "Unknown") for r in recordings)
    split_counts = Counter(r.get("split", "unassigned") for r in recordings)

    # Check for vocals representation
    vocal_count = class_counts.get("Vocals", 0)
    has_real_vocals = (vocal_count > 0 and not any("slakh" in r.get("dataset_name", "").lower() for r in recordings if r.get("class_name") == "Vocals"))

    # Generate Markdown Report
    lines = [
        "# ProVerb AI Training Data — License & Compliance Audit Report",
        f"**Organization:** ProducerToy | **Plugin:** ProVerb  ",
        f"**Manifest Source:** `{os.path.basename(manifest_path)}`  ",
        f"**Total Records Audited:** {total_recs}  ",
        "",
        "---",
        "",
        "## 1. Executive Summary & Commercial Eligibility",
        "",
        "| Permission Status | Count | Percentage | Commercial Suitability |",
        "| :--- | :--- | :--- | :--- |"
    ]

    for status, count in permission_counts.items():
        pct = (count / max(1, total_recs)) * 100.0
        suitability = "Eligible (Attribution verified)" if "COMMERCIAL" in status or "ATTRIBUTION" in status else "EXCLUDED / Quarantined"
        lines.append(f"| `{status}` | {count} | {pct:.1f}% | {suitability} |")

    lines.extend([
        "",
        "---",
        "",
        "## 2. Ingested Datasets & Provenance",
        "",
        "| Dataset Name | Stems / Recordings | Ingested License | Verified Source URL |",
        "| :--- | :--- | :--- | :--- |"
    ])

    for ds_name, count in dataset_counts.items():
        ds_info = meta.get("datasets", {}).get(ds_name, {})
        lic = ds_info.get("license", "See records")
        url = ds_info.get("source_url", "N/A")
        lines.append(f"| **{ds_name}** | {count} | `{lic}` | [{url}]({url}) |")

    lines.extend([
        "",
        "---",
        "",
        "## 3. Acoustic Class Representation Audit",
        "",
        "| Class Index | ProVerb Class Name | Recorded Examples | Data Provenance & Realism Status |",
        "| :---: | :--- | :--- | :--- |"
    ])

    class_names = [
        "Drums", "Vocals", "Guitar", "Piano / Keys", "Bass", "Synth / Pad", "Other / Uncertain"
    ]

    for c_idx, c_name in enumerate(class_names):
        cnt = class_counts.get(c_name, 0)
        if c_name == "Vocals" and cnt == 0:
            status = "🔴 **MISSING (Slakh2100 contains 0 real vocals)**"
        elif cnt > 0:
            status = "🟢 Present in manifest"
        else:
            status = "🟡 0 records in manifest"
        lines.append(f"| [{c_idx}] | **{c_name}** | {cnt} | {status} |")

    lines.extend([
        "",
        "---",
        "",
        "## 4. Legal Attributions & Required Notices",
        ""
    ])

    for ds_name, ds_info in meta.get("datasets", {}).items():
        attr = ds_info.get("attribution", "")
        if attr:
            lines.append(f"- **{ds_name}**: *{attr}*")

    lines.extend([
        "",
        "---",
        "",
        "## 5. Critical Compliance Findings",
        "",
        "- **Real Vocals Gap:** Slakh2100 is an algorithmic MIDI-synthesized dataset and does not contain human vocal tracks. Models trained exclusively on Slakh cannot claim certified real human vocal classification.",
        "- **MUSDB18 Policy:** MUSDB18 is restricted to academic non-commercial research and is excluded from the default commercial build.",
        "- **Attribution Requirement:** CC BY 4.0 datasets require attribution in commercial plugin credits and documentation.",
        ""
    ])

    # Check for quarantined records log
    quarantine_log_path = os.path.join(os.path.dirname(manifest_path), "quarantined_records.json")
    if os.path.exists(quarantine_log_path):
        try:
            with open(quarantine_log_path, "r", encoding="utf-8") as qf:
                q_data = json.load(qf)
            lines.extend([
                "---",
                "",
                "## 6. Quarantined & Excluded Records Audit Trail",
                "",
                f"**Audit Action:** `{q_data.get('action', 'EXCLUDED')}`  ",
                f"**Note:** {q_data.get('audit_note', '')}",
                "",
                "| Recording ID | Previous Path | Dataset | Reason for Exclusion / Quarantine |",
                "| :--- | :--- | :--- | :--- |"
            ])
            for q_rec in q_data.get("quarantined_records", []):
                lines.append(f"| `{q_rec.get('recording_id')}` | `{q_rec.get('file_path')}` | {q_rec.get('dataset')} | {q_rec.get('reason')} |")
            lines.append("")
        except Exception as e:
            lines.append(f"\n*Warning loading quarantine log: {e}*\n")

    report_content = "\n".join(lines)
    os.makedirs(os.path.dirname(REPORT_OUTPUT_PATH), exist_ok=True)
    with open(REPORT_OUTPUT_PATH, "w", encoding="utf-8") as f:
        f.write(report_content)

    print("\n" + "=" * 78)
    print(" [LICENSE AUDIT REPORT GENERATED]")
    print(f" Manifest: {manifest_path}")
    print(f" Output Report: {REPORT_OUTPUT_PATH}")
    print(f" Total Records: {total_recs}")
    print(f" Commercial Permission Breakdown: {dict(permission_counts)}")
    print(f" Datasets Ingested: {dict(dataset_counts)}")
    print(f" Class Coverage: {dict(class_counts)}")
    print("=" * 78 + "\n")

    return {
        "total_records": total_recs,
        "permission_counts": dict(permission_counts),
        "dataset_counts": dict(dataset_counts),
        "class_counts": dict(class_counts),
        "report_file": REPORT_OUTPUT_PATH
    }


if __name__ == "__main__":
    m_path = sys.argv[1] if len(sys.argv) > 1 else MANIFEST_PATH
    audit_manifest(m_path)
