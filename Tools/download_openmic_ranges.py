#!/usr/bin/env python3
"""Parallel, resumable byte-range fetcher for the official OpenMIC archive.

The assembled archive is checked against Zenodo's exact size and MD5 before it
is handed to download_openmic.py for the existing safe extraction workflow.
"""

import concurrent.futures
import os
import sys
import time
import urllib.request

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS_DIR)
from download_openmic import compute_md5, query_official_zenodo_metadata  # noqa: E402

PROJECT_ROOT = os.path.abspath(os.path.join(TOOLS_DIR, ".."))
STAGING_DIR = os.path.join(PROJECT_ROOT, "Data", "staging")
ARCHIVE_PATH = os.path.join(STAGING_DIR, "openmic-2018-v1.0.0.tgz")
PART_PATH = ARCHIVE_PATH + ".part"
CHUNK_DIR = os.path.join(STAGING_DIR, "openmic-2018-ranges")
CHUNK_SIZE = 64 * 1024 * 1024
WORKERS = 8
USER_AGENT = "Mozilla/5.0 ProVerb-OpenMIC-Range-Downloader/1.0"


def fetch_range(url: str, index: int, start: int, end: int, total: int) -> tuple[int, str]:
    expected = end - start + 1
    chunk_path = os.path.join(CHUNK_DIR, f"chunk-{index:04d}.bin")
    temp_path = chunk_path + ".tmp"
    if os.path.isfile(chunk_path) and os.path.getsize(chunk_path) == expected:
        return expected, chunk_path

    for attempt in range(1, 7):
        try:
            req = urllib.request.Request(
                url,
                headers={"Range": f"bytes={start}-{end}", "Accept-Encoding": "identity", "User-Agent": USER_AGENT},
            )
            with urllib.request.urlopen(req, timeout=120) as response:
                content_range = response.headers.get("Content-Range", "")
                wanted_range = f"bytes {start}-{end}/{total}"
                if response.status != 206 or content_range != wanted_range:
                    raise RuntimeError(
                        f"Range {index} returned status={response.status}, Content-Range={content_range!r}; "
                        f"expected {wanted_range!r}"
                    )

                received = 0
                with open(temp_path, "wb") as out:
                    while True:
                        block = response.read(1024 * 1024)
                        if not block:
                            break
                        out.write(block)
                        received += len(block)
                    out.flush()
                    os.fsync(out.fileno())

            if received != expected:
                raise IOError(f"Range {index} length mismatch: expected {expected}, got {received}")
            os.replace(temp_path, chunk_path)
            return received, chunk_path
        except Exception:
            if os.path.exists(temp_path):
                os.remove(temp_path)
            if attempt == 6:
                raise
            time.sleep(min(15, attempt * 2))

    raise RuntimeError(f"Range {index} failed unexpectedly")


def main() -> None:
    metadata = query_official_zenodo_metadata()
    expected_size = int(metadata["size_bytes"])
    expected_md5 = metadata["md5"].lower()
    os.makedirs(CHUNK_DIR, exist_ok=True)
    os.makedirs(STAGING_DIR, exist_ok=True)

    ranges = []
    for index, start in enumerate(range(0, expected_size, CHUNK_SIZE)):
        end = min(start + CHUNK_SIZE, expected_size) - 1
        ranges.append((index, start, end))

    print(f"[*] Fetching {len(ranges)} verified byte ranges with {WORKERS} workers.", flush=True)
    results = {}
    completed = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=WORKERS) as pool:
        futures = {
            pool.submit(fetch_range, metadata["download_url"], idx, start, end, expected_size): idx
            for idx, start, end in ranges
        }
        for future in concurrent.futures.as_completed(futures):
            idx = futures[future]
            size, path = future.result()
            results[idx] = path
            completed += size
            print(f"    Range {idx + 1:02d}/{len(ranges)} complete — {completed / (1024**2):.1f} / {expected_size / (1024**2):.1f} MiB", flush=True)

    with open(PART_PATH, "wb") as assembled:
        for idx, _, _ in ranges:
            with open(results[idx], "rb") as chunk:
                while True:
                    block = chunk.read(8 * 1024 * 1024)
                    if not block:
                        break
                    assembled.write(block)
        assembled.flush()
        os.fsync(assembled.fileno())

    actual_size = os.path.getsize(PART_PATH)
    if actual_size != expected_size:
        raise RuntimeError(f"Assembled archive size mismatch: expected {expected_size}, got {actual_size}")
    actual_md5 = compute_md5(PART_PATH)
    if actual_md5.lower() != expected_md5:
        raise RuntimeError(f"Assembled archive MD5 mismatch: expected {expected_md5}, got {actual_md5}")

    os.replace(PART_PATH, ARCHIVE_PATH)
    for idx, _, _ in ranges:
        os.remove(results[idx])
    for name in os.listdir(CHUNK_DIR):
        path = os.path.join(CHUNK_DIR, name)
        if os.path.isfile(path) and name.endswith(".tmp"):
            os.remove(path)
    print(f"[OK] Verified {ARCHIVE_PATH} (MD5 {actual_md5}). Run download_openmic.py to extract safely.", flush=True)


if __name__ == "__main__":
    main()
