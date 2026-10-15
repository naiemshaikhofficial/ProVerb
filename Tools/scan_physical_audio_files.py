import os
import glob
from collections import Counter

DATA_ROOT = 'd:/Website/ProducerToy-Plugins/ProVerb/Data'

def scan_audio(dir_path):
    exts = ('.wav', '.mp3', '.ogg', '.flac')
    count = 0
    found_exts = Counter()
    for root, _, files in os.walk(dir_path):
        for f in files:
            e = os.path.splitext(f)[1].lower()
            if e in exts:
                count += 1
                found_exts[e] += 1
    return count, found_exts

def main():
    print("=== PHYSICAL AUDIO FILES ON DISK AUDIT ===")
    for d in sorted(os.listdir(DATA_ROOT)):
        p = os.path.join(DATA_ROOT, d)
        if os.path.isdir(p):
            c, exts = scan_audio(p)
            print(f"Directory: {d:<12} | Total audio files: {c:6d} | Extensions: {dict(exts)}")

if __name__ == '__main__':
    main()
