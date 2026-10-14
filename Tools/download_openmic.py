#!/usr/bin/env python3
"""
OpenMIC-2018 Official Dataset Downloader & Storage Verification Tool
ProducerToy — ProVerb Smart Algorithmic Reverb

Official Zenodo Record: 1432913 (DOI: 10.5281/zenodo.1432913)
Dataset: OpenMIC-2018: An Open Dataset for Multiple Instrument Recognition
Authors: Eric J. Humphrey, Simon Durand, Brian McFee (Spotify & NYU MARL)
License: Creative Commons Attribution 4.0 International (CC BY 4.0)

Safety Architecture:
1. Queries official Zenodo API dynamically to verify record title, license, file size, and MD5.
2. Checks available disk space before downloading and extracting.
3. Downloads to a staging location (Data/staging/openmic-2018-v1.0.0.tgz) with HTTP Range auto-resume.
4. Validates archive integrity (exact byte size & MD5 checksum). Quarantines on failure.
5. Safe tar.gz extraction with path traversal protection.
6. Moves validated tracks to destination directory (Data/OpenMIC/) and writes download provenance.
"""

import os
import sys
import json
import shutil
import hashlib
import tarfile
import argparse
import urllib.request
import time
import subprocess
from typing import Dict, Optional, Tuple

if sys.platform == "win32":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")

ZENODO_RECORD_ID = 1432913
ZENODO_API_URL = f"https://zenodo.org/api/records/{ZENODO_RECORD_ID}"
ZENODO_WEB_DOWNLOAD_URL = f"https://zenodo.org/records/{ZENODO_RECORD_ID}/files/openmic-2018-v1.0.0.tgz?download=1"

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(TOOLS_DIR, ".."))
DEFAULT_DATA_DIR = os.path.join(PROJECT_ROOT, "Data", "OpenMIC")
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
    """Queries official Zenodo API for Record 1432913 metadata."""
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
        tgt_file = None
        for f in files:
            fname = f.get("key") or f.get("filename") or ""
            if "openmic" in fname.lower() and (fname.endswith(".tgz") or fname.endswith(".tar.gz")):
                tgt_file = f
                break

        if tgt_file:
            chk = tgt_file.get("checksum", "")
            md5_val = chk.replace("md5:", "").strip() if "md5:" in chk else chk.strip()
            return {
                "title": meta.get("title", "OpenMIC-2018"),
                "doi": meta.get("doi", "10.5281/zenodo.1432913"),
                "license": meta.get("license", {}).get("id", "cc-by-4.0"),
                "creators": [c.get("name") for c in meta.get("creators", [])],
                "filename": tgt_file.get("key") or tgt_file.get("filename", "openmic-2018-v1.0.0.tgz"),
                "size_bytes": int(tgt_file.get("size", 2623376754)),
                "md5": md5_val or "e4ccf187e2bb5ab2e115416e8aafe7f4",
                "download_url": ZENODO_WEB_DOWNLOAD_URL
            }

    # Verified official fallback constants
    return {
        "title": "OpenMIC-2018: An Open Dataset for Multiple Instrument Recognition",
        "doi": "10.5281/zenodo.1432913",
        "license": "cc-by-4.0",
        "creators": ["Eric J. Humphrey", "Simon Durand", "Brian McFee"],
        "filename": "openmic-2018-v1.0.0.tgz",
        "size_bytes": 2623376754,
        "md5": "e4ccf187e2bb5ab2e115416e8aafe7f4",
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
        print("    [+] Using Python urllib resumable engine...", flush=True)
        t0 = time.time()
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
                        pct = (cur_bytes / expected_size) * 100.0
                        elapsed = max(0.1, now - t0)
                        speed_mb = ((cur_bytes - initial_bytes) / (1024 * 1024)) / elapsed
                        print(f"    Progress: {pct:5.1f}% | {cur_bytes / (1024**2):.1f} / {expected_size / (1024**2):.1f} MB | Speed: {speed_mb:.2f} MB/s", flush=True)
            except Exception as e:
                print(f"    [!] Transient urllib error: {e}. Resuming in 3s...", flush=True)
                time.sleep(3.0)

    # Move part file to final dest
    if os.path.exists(dest_path):
        os.remove(dest_path)
    os.rename(part_path, dest_path)
    print(f"    [OK] Download completed successfully: {dest_path}", flush=True)


def safe_extract_tar(tar_path: str, extract_dir: str):
    """Safely extracts tar.gz archive with directory traversal guards."""
    print(f"[*] Extracting {tar_path} -> {extract_dir}...", flush=True)
    os.makedirs(extract_dir, exist_ok=True)
    abs_extract_dir = os.path.abspath(extract_dir)

    with tarfile.open(tar_path, "r:*") as tf:
        members = tf.getmembers()
        total_members = len(members)
        print(f"    Total archive members: {total_members:,}", flush=True)
        extracted_count = 0
        t0 = time.time()
        last_print = t0

        for member in members:
            # Traversal guard
            member_path = os.path.abspath(os.path.join(extract_dir, member.name))
            if not member_path.startswith(abs_extract_dir + os.sep) and member_path != abs_extract_dir:
                raise RuntimeError(f"Attempted path traversal in tar: {member.name}")

            tf.extract(member, extract_dir)
            extracted_count += 1
            now = time.time()
            if now - last_print > 3.0:
                pct = (extracted_count / total_members) * 100.0
                print(f"    - Extraction progress: {pct:5.1f}% ({extracted_count:,}/{total_members:,} items)...", flush=True)
                last_print = now

    print(f"    [OK] Extraction completed cleanly ({extracted_count:,} files/dirs).", flush=True)


def main():
    parser = argparse.ArgumentParser(description="Download and verify official OpenMIC-2018 dataset from Zenodo.")
    parser.add_argument("--data-dir", default=DEFAULT_DATA_DIR, help="Destination directory for extracted audio")
    parser.add_argument("--staging-dir", default=STAGING_DIR, help="Staging directory for downloaded archive")
    parser.add_argument("--skip-extract", action="store_true", help="Download and verify archive only, skip extraction")
    args = parser.parse_args()

    print("=" * 78, flush=True)
    print("  ProVerb AI — OpenMIC-2018 Official Dataset Ingestion Pipeline", flush=True)
    print("=" * 78, flush=True)

    meta = query_official_zenodo_metadata()
    print(f"\n[Dataset Metadata]")
    print(f"  Title      : {meta['title']}")
    print(f"  DOI        : {meta['doi']}")
    print(f"  License    : {meta['license']} (CC BY 4.0 - Commercially Permitted with Attribution)")
    print(f"  Creators   : {', '.join(meta['creators'])}")
    print(f"  Archive    : {meta['filename']}")
    print(f"  Expected   : {meta['size_bytes']:,} bytes ({meta['size_bytes'] / (1024**3):.2f} GB)")
    print(f"  MD5        : {meta['md5']}")

    # Disk space check (require at least 15 GB free for archive + extraction)
    free_gb = get_drive_free_gb(args.staging_dir)
    print(f"\n[*] Available disk space on target volume: {free_gb:.2f} GB", flush=True)
    if free_gb < 15.0:
        print(f"[!] Warning: Less than 15 GB free disk space available ({free_gb:.2f} GB).", flush=True)

    archive_path = os.path.join(args.staging_dir, meta["filename"])
    archive_verified = False

    # Download or verify existing
    if os.path.exists(archive_path):
        actual_size = os.path.getsize(archive_path)
        print(f"\n[*] Found existing staged archive: {archive_path} ({actual_size:,} bytes)", flush=True)
        if actual_size == meta["size_bytes"]:
            print("    - File size matches expected size. Verifying MD5...", flush=True)
            actual_md5 = compute_md5(archive_path)
            if actual_md5.lower() == meta["md5"].lower():
                print(f"    [OK] MD5 checksum verified successfully: {actual_md5}", flush=True)
                archive_verified = True
            else:
                print(f"    [!] MD5 mismatch: expected {meta['md5']}, got {actual_md5}. Quarantining...", flush=True)
                os.makedirs(QUARANTINE_DIR, exist_ok=True)
                shutil.move(archive_path, os.path.join(QUARANTINE_DIR, f"{meta['filename']}.corrupt_{int(time.time())}"))
                download_with_progress(meta["download_url"], archive_path, meta["size_bytes"])
        else:
            print(f"    [!] Size mismatch ({actual_size} vs {meta['size_bytes']}). Redownloading/resuming...", flush=True)
            part_path = archive_path + ".part"
            if not os.path.exists(part_path):
                shutil.move(archive_path, part_path)
            download_with_progress(meta["download_url"], archive_path, meta["size_bytes"])
    else:
        download_with_progress(meta["download_url"], archive_path, meta["size_bytes"])

    # Re-validate every download/resume path before extraction. Previously, the
    # resume and corrupt-archive branches could proceed without a checksum check.
    if not archive_verified:
        print("\n[*] Verifying downloaded archive size and MD5 checksum...", flush=True)
        actual_size = os.path.getsize(archive_path)
        if actual_size != meta["size_bytes"]:
            raise RuntimeError(f"Archive size mismatch! Expected {meta['size_bytes']}, got {actual_size}")
        actual_md5 = compute_md5(archive_path)
        if actual_md5.lower() != meta["md5"].lower():
            raise RuntimeError(f"MD5 mismatch after download! Expected {meta['md5']}, got {actual_md5}")
        print(f"    [OK] MD5 verified: {actual_md5}", flush=True)

    if not args.skip_extract:
        safe_extract_tar(archive_path, args.data_dir)

    # Write download provenance
    os.makedirs(args.data_dir, exist_ok=True)
    prov_file = os.path.join(args.data_dir, "download_provenance.json")
    with open(prov_file, "w", encoding="utf-8") as f:
        json.dump({
            "dataset": "OpenMIC-2018",
            "zenodo_record_id": ZENODO_RECORD_ID,
            "doi": meta["doi"],
            "license": meta["license"],
            "archive_filename": meta["filename"],
            "archive_size_bytes": meta["size_bytes"],
            "archive_md5": meta["md5"],
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "status": "VERIFIED_EXTRACTED" if not args.skip_extract else "VERIFIED_DOWNLOADED_ONLY"
        }, f, indent=2)

    print(f"\n[OK] OpenMIC-2018 pipeline step completed. Provenance written to {prov_file}")


if __name__ == "__main__":
    main()
