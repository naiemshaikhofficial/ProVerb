#!/usr/bin/env python3
"""Download and safely extract the selected CC-BY instrument datasets."""

import hashlib
import json
import os
import shutil
import subprocess
import tarfile
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
STAGING = os.path.join(ROOT, "Data", "staging")

DATASETS = [
    {
        "name": "GuitarSet",
        "record_id": "3371780",
        "filename": "audio_mono-pickup_mix.zip",
        "size": 683145360,
        "md5": "aecce79f425a44e2055e46f680e10f6a",
        "destination": os.path.join(ROOT, "Data", "GuitarSet"),
        "marker": ".guitarset-audio-mono-pickup-mix-extracted.json",
    },
    {
        "name": "WaivOps-SYN-SE1",
        "record_id": "13993751",
        "filename": "syn_se1_id_001_wav.tar.gz",
        "size": 75595614,
        "md5": "147109b5ce53186b878e88c4d2adc44b",
        "destination": os.path.join(ROOT, "Data", "SYN-SE1"),
        "marker": ".syn-se1-extracted.json",
    },
]


def md5_file(path):
    digest = hashlib.md5()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download(item):
    os.makedirs(STAGING, exist_ok=True)
    final_path = os.path.join(STAGING, item["filename"])
    partial_path = final_path + ".part"
    if os.path.isfile(final_path) and md5_file(final_path) == item["md5"]:
        print(f"[OK] Verified existing archive: {final_path}", flush=True)
        return final_path

    if os.path.isfile(final_path):
        os.replace(final_path, final_path + ".invalid")
    if os.path.isfile(partial_path) and item.get("size") and os.path.getsize(partial_path) > item["size"]:
        os.replace(partial_path, partial_path + ".oversized")

    url = (f"https://zenodo.org/records/{item['record_id']}/files/"
           f"{item['filename']}?download=1")
    print(f"[*] Downloading {item['name']} from the official Zenodo record", flush=True)
    command = [
        "curl.exe", "--location", "--fail", "--show-error", "--silent",
        "--retry", "5", "--retry-delay", "2", "--retry-max-time", "300",
        "--connect-timeout", "60", "--continue-at", "-",
        "--output", partial_path,
    ]
    command.append(url)
    result = subprocess.run(command, check=False)
    if result.returncode:
        raise RuntimeError(
            f"curl.exe failed for {item['filename']} (exit {result.returncode}); "
            f"partial data, if any, is retained at {partial_path}"
        )
    actual = md5_file(partial_path)
    if item.get("size") and os.path.getsize(partial_path) != item["size"]:
        raise RuntimeError(
            f"Size mismatch for {item['filename']}: expected {item['size']}, "
            f"got {os.path.getsize(partial_path)}. Partial file retained."
        )
    if actual != item["md5"]:
        raise RuntimeError(
            f"MD5 mismatch for {item['filename']}: expected {item['md5']}, got {actual}. "
            "The partial file is retained for diagnosis/resume."
        )
    os.replace(partial_path, final_path)
    print(f"[OK] MD5 verified: {final_path}", flush=True)
    return final_path


def _safe_target(root, member_name):
    target = os.path.realpath(os.path.join(root, member_name))
    root_real = os.path.realpath(root)
    if os.path.commonpath((root_real, target)) != root_real:
        raise RuntimeError(f"Archive path escapes extraction directory: {member_name}")
    return target


def extract(archive_path, item):
    destination = item["destination"]
    marker_path = os.path.join(destination, item["marker"])
    if os.path.isfile(marker_path):
        print(f"[OK] Already extracted: {destination}", flush=True)
        return
    if os.path.exists(destination) and os.listdir(destination):
        raise RuntimeError(
            f"Refusing to extract over existing unmarked directory: {destination}"
        )
    os.makedirs(destination, exist_ok=True)

    if zipfile.is_zipfile(archive_path):
        with zipfile.ZipFile(archive_path) as archive:
            for member in archive.infolist():
                _safe_target(destination, member.filename)
                unix_mode = member.external_attr >> 16
                if (unix_mode & 0o170000) == 0o120000:
                    raise RuntimeError(f"Refusing archive symlink: {member.filename}")
            archive.extractall(destination)
    elif tarfile.is_tarfile(archive_path):
        with tarfile.open(archive_path, "r:gz") as archive:
            for member in archive.getmembers():
                _safe_target(destination, member.name)
                if member.issym() or member.islnk() or not (member.isfile() or member.isdir()):
                    raise RuntimeError(f"Refusing unsupported tar entry: {member.name}")
            archive.extractall(destination)
    else:
        raise RuntimeError(f"Unsupported archive format: {archive_path}")

    with open(marker_path, "w", encoding="utf-8") as marker:
        json.dump({"archive": os.path.basename(archive_path), "md5": item["md5"]}, marker, indent=2)
    print(f"[OK] Extracted {item['name']} to {destination}", flush=True)


def main():
    for item in DATASETS:
        archive_path = download(item)
        extract(archive_path, item)
        if item["name"] == "WaivOps-SYN-SE1":
            metadata = {
                "name": item["name"] + " metadata",
                "record_id": item["record_id"],
                "filename": "syn_se1_id_001.json",
                "size": 125862,
                "md5": "7c4edb0c528cf4f1efb36ed67441795d",
            }
            metadata_path = download(metadata)
            destination_path = os.path.join(item["destination"], metadata["filename"])
            if not os.path.isfile(destination_path) or md5_file(destination_path) != metadata["md5"]:
                shutil.copy2(metadata_path, destination_path)
            print(f"[OK] SYN-SE1 metadata ready: {destination_path}", flush=True)


if __name__ == "__main__":
    main()
