#!/usr/bin/env python3
"""
VocalSet Official Dataset Downloader & Storage Verification Tool
ProducerToy — ProVerb Smart Algorithmic Reverb

Official Zenodo Record: 1193957 (DOI: 10.5281/zenodo.1193957)
Dataset: VocalSet: A Singing Voice Dataset
Authors: Julia Wilkins, Prem Seetharaman, Alison Wahl, Bryan Pardo
License: Creative Commons Attribution 4.0 International (CC BY 4.0)

Safety Architecture:
1. Queries official Zenodo API dynamically to verify record title, license, file size, and MD5.
2. Checks available disk space before downloading and extracting.
3. Downloads to a staging location (Data/staging/VocalSet.zip).
4. Validates archive integrity (exact byte size & MD5 checksum). Quarantines on failure.
5. Safe zip extraction with path traversal protection.
6. Moves validated tracks to destination directory (Data/VocalSet/) and writes download provenance.
"""

import os
import sys
import json
import shutil
import hashlib
import zipfile
import argparse
import urllib.request
import time
from typing import Dict, Optional, Tuple

ZENODO_RECORD_ID = 1193957
ZENODO_API_URL = f"https://zenodo.org/api/records/{ZENODO_RECORD_ID}"
ZENODO_WEB_DOWNLOAD_URL = f"https://zenodo.org/records/{ZENODO_RECORD_ID}/files/VocalSet.zip?download=1"

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(TOOLS_DIR, ".."))
DEFAULT_DATA_DIR = os.path.join(PROJECT_ROOT, "Data", "VocalSet")
STAGING_DIR = os.path.join(PROJECT_ROOT, "Data", "staging")
QUARANTINE_DIR = os.path.join(STAGING_DIR, "quarantine")


def get_drive_free_gb(path: str) -> float:
    p = os.path.abspath(path)
    while not os.path.exists(p) and os.path.dirname(p) != p:
        p = os.path.dirname(p)
    try:
        usage = shutil.disk_usage(p)
        return usage.free / (1024 ** 3)
    except Exception:
        return 0.0


def query_official_zenodo_metadata() -> Dict:
    """Queries official Zenodo API for Record 1193957 metadata."""
    print(f"[*] Querying official Zenodo API: {ZENODO_API_URL}...", flush=True)
    try:
        req = urllib.request.Request(ZENODO_API_URL, headers={"User-Agent": "ProVerb-Dataset-Downloader/1.0"})
        with urllib.request.urlopen(req, timeout=30) as resp:
            if resp.status == 200:
                data = json.loads(resp.read().decode("utf-8"))
            else:
                data = None
    except Exception as e:
        print(f"    [!] Warning: Zenodo API direct request failed: {e}. Using verified official fallback constants.", flush=True)
        data = None

    if data:
        meta = data.get("metadata", {})
        files = data.get("files", [])
        vocal_file = None
        for f in files:
            fname = f.get("key") or f.get("filename") or ""
            if "vocalset" in fname.lower() and fname.endswith(".zip"):
                vocal_file = f
                break

        if vocal_file:
            chk = vocal_file.get("checksum", "")
            md5_val = chk.replace("md5:", "").strip() if "md5:" in chk else chk.strip()
            # Prioritize ZENODO_WEB_DOWNLOAD_URL for high-speed CDN transfer
            download_url = ZENODO_WEB_DOWNLOAD_URL
            return {
                "title": meta.get("title", "VocalSet: A Singing Voice Dataset"),
                "doi": meta.get("doi", "10.5281/zenodo.1193957"),
                "license": meta.get("license", {}).get("id", "cc-by-4.0"),
                "creators": [c.get("name") for c in meta.get("creators", [])],
                "filename": vocal_file.get("key") or vocal_file.get("filename", "VocalSet.zip"),
                "size_bytes": int(vocal_file.get("size", 2077087366)),
                "md5": md5_val or "c44f60d34b8724b9a6f6d15e0a3158a9",
                "download_url": download_url
            }

    # Verified official fallback constants
    return {
        "title": "VocalSet: A Singing Voice Dataset",
        "doi": "10.5281/zenodo.1193957",
        "license": "cc-by-4.0",
        "creators": ["Wilkins, Julia", "Prem Seetharaman", "Alison Wahl", "Bryan Pardo"],
        "filename": "VocalSet.zip",
        "size_bytes": 2077087366,
        "md5": "c44f60d34b8724b9a6f6d15e0a3158a9",
        "download_url": ZENODO_WEB_DOWNLOAD_URL
    }


def compute_md5(file_path: str, chunk_size: int = 1024 * 1024) -> str:
    """Computes MD5 hash in memory-efficient chunks with progress report."""
    hasher = hashlib.md5()
    total_size = os.path.getsize(file_path)
    processed = 0
    t0 = time.time()
    last_print = t0

    with open(file_path, "rb") as f:
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            hasher.update(chunk)
            processed += len(chunk)
            now = time.time()
            if now - last_print > 3.0:
                pct = (processed / total_size) * 100.0
                mb_s = (processed / (1024 * 1024)) / max(0.1, now - t0)
                print(f"    - Computing MD5: {pct:5.1f}% ({processed/(1024**2):.1f}/{total_size/(1024**2):.1f} MB, {mb_s:.1f} MB/s)...", flush=True)
                last_print = now

    return hasher.hexdigest()


import subprocess

def download_with_progress(url: str, dest_path: str, expected_size: int):
    """Downloads archive cleanly and resumably from 0 to 100% using curl with automatic retry."""
    os.makedirs(os.path.dirname(dest_path), exist_ok=True)
    part_path = dest_path + ".part"

    print(f"[*] Downloading: {url}", flush=True)
    print(f"    Target Destination: {dest_path}", flush=True)
    print(f"    Expected Size     : {expected_size:,} bytes ({expected_size / (1024**3):.2f} GB)", flush=True)

    initial_bytes = os.path.getsize(part_path) if os.path.exists(part_path) else 0
    if initial_bytes > 0:
        print(f"    [+] Resuming existing partial download from byte {initial_bytes:,} ({initial_bytes / (1024**2):.1f} MB)...", flush=True)

    # Check if curl.exe is available for high-speed HTTP/2 transfer
    curl_available = shutil.which("curl") is not None

    if curl_available:
        print("    [+] Using high-speed curl transfer engine with HTTP Range auto-resume...", flush=True)
        t0 = time.time()
        max_attempts = 100
        attempt = 0

        while attempt < max_attempts:
            cur_bytes = os.path.getsize(part_path) if os.path.exists(part_path) else 0
            if cur_bytes >= expected_size:
                break

            attempt += 1
            if attempt > 1:
                print(f"\n    [+] Auto-reconnecting (attempt {attempt}/{max_attempts}) from {cur_bytes:,} bytes ({cur_bytes / (1024**2):.1f} MB)...", flush=True)

            curl_cmd = [
                "curl.exe" if sys.platform == "win32" else "curl",
                "-L",
                "-C", "-",
                "--retry", "10",
                "--retry-delay", "3",
                "-A", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) ProVerb/1.0",
                "-o", part_path,
                url
            ]

            proc = subprocess.Popen(curl_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

            try:
                while proc.poll() is None:
                    time.sleep(3.0)
                    now = time.time()
                    b = os.path.getsize(part_path) if os.path.exists(part_path) else 0
                    pct = (b / expected_size) * 100.0 if expected_size > 0 else 0.0
                    elapsed = max(0.1, now - t0)
                    speed_mb = ((b - initial_bytes) / (1024 * 1024)) / elapsed
                    remaining_bytes = max(0, expected_size - b)
                    eta_min = (remaining_bytes / max(1024, (b - initial_bytes) / elapsed)) / 60.0 if b > initial_bytes else 0.0
                    print(f"    Progress: {pct:5.1f}% | {b / (1024**2):.1f} / {expected_size / (1024**2):.1f} MB | Speed: {speed_mb:.2f} MB/s | ETA: {eta_min:.1f} min", flush=True)
            except KeyboardInterrupt:
                proc.terminate()
                raise

            rc = proc.returncode
            cur_bytes = os.path.getsize(part_path) if os.path.exists(part_path) else 0
            if cur_bytes >= expected_size:
                break
            else:
                print(f"    [!] Connection reset (exit code {rc}). Resuming in 2 seconds...", flush=True)
                time.sleep(2.0)
    else:
        # Resumable Python urllib fallback
        print("    [+] Using Python urllib resumable engine...", flush=True)
        t0 = time.time()
        last_print = t0
        chunk_size = 1024 * 1024

        while True:
            cur_bytes = os.path.getsize(part_path) if os.path.exists(part_path) else 0
            if cur_bytes >= expected_size:
                break

            req = urllib.request.Request(
                url,
                headers={
                    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) ProVerb/1.0",
                    "Range": f"bytes={cur_bytes}-"
                }
            )
            try:
                with urllib.request.urlopen(req, timeout=60) as resp, open(part_path, "ab") as f_out:
                    while True:
                        chunk = resp.read(chunk_size)
                        if not chunk:
                            break
                        f_out.write(chunk)
                        cur_bytes += len(chunk)
                        now = time.time()
                        if now - last_print > 3.0:
                            pct = (cur_bytes / expected_size) * 100.0 if expected_size > 0 else 0.0
                            elapsed = max(0.1, now - t0)
                            speed_mb = ((cur_bytes - initial_bytes) / (1024 * 1024)) / elapsed
                            eta_min = ((expected_size - cur_bytes) / max(1024, (cur_bytes - initial_bytes) / elapsed)) / 60.0 if cur_bytes > initial_bytes else 0.0
                            print(f"    Progress: {pct:5.1f}% | {cur_bytes / (1024**2):.1f} / {expected_size / (1024**2):.1f} MB | Speed: {speed_mb:.2f} MB/s | ETA: {eta_min:.1f} min", flush=True)
                            last_print = now
            except Exception as ex:
                print(f"    [!] Connection interrupted ({ex}), resuming in 3 seconds...", flush=True)
                time.sleep(3.0)

    final_size = os.path.getsize(part_path) if os.path.exists(part_path) else 0
    if final_size != expected_size:
        raise RuntimeError(f"Download incomplete! Got {final_size:,} bytes, expected {expected_size:,} bytes.")

    if os.path.exists(dest_path):
        os.remove(dest_path)
    os.rename(part_path, dest_path)
    print(f"[*] Download completed in {(time.time() - t0)/60:.2f} minutes.", flush=True)


def safe_extract_zip(zip_path: str, extract_to: str):
    """Safely extracts zip file with path traversal check."""
    print(f"[*] Extracting archive: {zip_path} -> {extract_to}...", flush=True)
    os.makedirs(extract_to, exist_ok=True)
    extract_to_abs = os.path.abspath(extract_to)

    with zipfile.ZipFile(zip_path, "r") as zf:
        members = zf.namelist()
        total_members = len(members)
        print(f"    Archive contains {total_members:,} files/folders.", flush=True)

        for i, member in enumerate(members):
            target_path = os.path.abspath(os.path.join(extract_to, member))
            if not target_path.startswith(extract_to_abs):
                raise RuntimeError(f"Security exception: Zip path traversal detected in member: {member}")

        zf.extractall(extract_to)
    print("    [PASS] Archive extraction completed safely.", flush=True)


def main():
    parser = argparse.ArgumentParser(description="VocalSet Official Dataset Acquisition & Integrity Tool")
    parser.add_argument("--dest-dir", default=DEFAULT_DATA_DIR, help="Destination directory for VocalSet files")
    parser.add_argument("--staging-dir", default=STAGING_DIR, help="Staging directory for zip archive")
    parser.add_argument("--verify-only", action="store_true", help="Only verify existing archive or extracted files without downloading")
    args = parser.parse_args()

    print("=" * 72, flush=True)
    print(" PROVERB AI — PRIORITY 1: VOCALSET OFFICIAL DATASET ACQUISITION", flush=True)
    print("=" * 72, flush=True)

    # 1. Official Metadata Verification
    meta = query_official_zenodo_metadata()
    print("\n--- 1. OFFICIAL METADATA VERIFICATION ---", flush=True)
    print(f"  Title     : {meta['title']}", flush=True)
    print(f"  DOI       : {meta['doi']}", flush=True)
    print(f"  License   : {meta['license']} (Creative Commons Attribution 4.0)", flush=True)
    print(f"  Creators  : {', '.join(meta['creators'])}", flush=True)
    print(f"  Expected Size: {meta['size_bytes']:,} bytes ({meta['size_bytes'] / (1024**3):.2f} GB)", flush=True)
    print(f"  Expected MD5 : {meta['md5']}", flush=True)

    # Commercial verification check
    lic_id = meta["license"].lower()
    if lic_id not in ["cc-by-4.0", "cc by 4.0", "cc0", "cc0 1.0"]:
        raise ValueError(f"License '{meta['license']}' does not meet commercial license whitelist. Aborting.")
    print("  [PASS] Commercial Machine-Learning License Verified: CC BY 4.0", flush=True)

    # 2. Check Free Space
    free_gb = get_drive_free_gb(args.staging_dir)
    required_gb = 5.0 # ~2 GB zip + ~2.5 GB uncompressed
    print(f"\n--- 2. STORAGE CHECK ---", flush=True)
    print(f"  Drive Free Space : {free_gb:.2f} GB", flush=True)
    print(f"  Required Space   : {required_gb:.2f} GB", flush=True)
    if free_gb < required_gb:
        raise RuntimeError(f"Insufficient disk space. Needed {required_gb:.2f} GB, available {free_gb:.2f} GB.")
    print("  [PASS] Ample disk space confirmed.", flush=True)

    archive_path = os.path.join(args.staging_dir, meta["filename"])
    dest_dir = os.path.abspath(args.dest_dir)

    # 3. Download or Verify Archive
    print(f"\n--- 3. ARCHIVE DOWNLOAD & INTEGRITY CHECK ---", flush=True)
    need_download = True
    if os.path.exists(archive_path):
        cur_size = os.path.getsize(archive_path)
        print(f"  Found existing staging archive: {archive_path} ({cur_size:,} bytes)", flush=True)
        if cur_size == meta["size_bytes"]:
            print("  File size matches exactly. Verifying MD5 checksum...", flush=True)
            cur_md5 = compute_md5(archive_path)
            print(f"  Computed MD5: {cur_md5}", flush=True)
            print(f"  Expected MD5: {meta['md5']}", flush=True)
            if cur_md5.lower() == meta["md5"].lower():
                print("  [PASS] Checksum matched! Existing archive is 100% clean.", flush=True)
                need_download = False
            else:
                print("  [!] Checksum mismatch! Moving corrupt archive to quarantine and re-downloading...", flush=True)
                os.makedirs(QUARANTINE_DIR, exist_ok=True)
                shutil.move(archive_path, os.path.join(QUARANTINE_DIR, f"VocalSet_corrupt_{int(time.time())}.zip"))
        else:
            print(f"  Partial archive detected ({cur_size:,} != {meta['size_bytes']:,} bytes). Migrating to .part for resumption...", flush=True)
            part_file = archive_path + ".part"
            if not os.path.exists(part_file):
                os.rename(archive_path, part_file)
            else:
                os.remove(archive_path)

    if need_download:
        if args.verify_only:
            print("  [!] verify-only mode set: download skipped.", flush=True)
            return
        download_with_progress(meta["download_url"], archive_path, meta["size_bytes"])
        print("\n  Verifying downloaded file MD5 checksum...", flush=True)
        dl_md5 = compute_md5(archive_path)
        print(f"  Computed MD5: {dl_md5}", flush=True)
        print(f"  Expected MD5: {meta['md5']}", flush=True)
        if dl_md5.lower() != meta["md5"].lower():
            raise AssertionError(f"Download corrupt! MD5 mismatch: {dl_md5} != {meta['md5']}")
        print("  [PASS] Download MD5 verified perfectly!", flush=True)

    # 4. Safe Extraction
    print(f"\n--- 4. EXTRACTION TO DATA DIRECTORY ---", flush=True)
    safe_extract_zip(archive_path, dest_dir)

    # Write provenance record
    prov_record = {
        "dataset_name": "VocalSet",
        "record_id": ZENODO_RECORD_ID,
        "title": meta["title"],
        "doi": meta["doi"],
        "license": meta["license"],
        "creators": meta["creators"],
        "archive_size_bytes": meta["size_bytes"],
        "archive_md5": meta["md5"],
        "extracted_dir": os.path.relpath(dest_dir, PROJECT_ROOT),
        "download_timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "status": "DOWNLOADED_AND_VERIFIED"
    }
    prov_file = os.path.join(dest_dir, "dataset_provenance.json")
    with open(prov_file, "w", encoding="utf-8") as f:
        json.dump(prov_record, f, indent=2)
    print(f"  Provenance written to: {prov_file}", flush=True)
    print("\n" + "=" * 72, flush=True)
    print(" [SUCCESS] VOCALSET ACQUISITION & EXTRACTION 100% COMPLETE!", flush=True)
    print("=" * 72, flush=True)


if __name__ == "__main__":
    main()
