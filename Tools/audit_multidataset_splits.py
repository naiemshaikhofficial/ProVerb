import os
import csv
import json
from collections import defaultdict, Counter

OPENMIC_META = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/openmic-2018-metadata.csv'
OPENMIC_TEST = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/partitions/split01_test.csv'
OPENMIC_TRAIN = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/partitions/split01_train.csv'
OPENMIC_RESPONSES = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/openmic-2018-aggregated-labels.csv'

GUITARSET_DIR = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/GuitarSet'
VOCALSET_DIR = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/VocalSet'
SYNSE1_DIR = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/SYN-SE1'
SLAKH_DIR = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/Slakh'

VERIFIED_COMMERCIAL_ALLOWLIST = {
    ('Attribution', 'http://creativecommons.org/licenses/by/4.0/'): 'CC-BY-4.0',
    ('Creative Commons Attribution', 'http://creativecommons.org/licenses/by/3.0/'): 'CC-BY-3.0',
    ('CC0 1.0 Universal', 'http://creativecommons.org/publicdomain/zero/1.0/'): 'CC0-1.0',
    ('Attribution 3.0 United States', 'http://creativecommons.org/licenses/by/3.0/us/'): 'CC-BY-3.0-US',
    ('Attribution 3.0 International', 'http://creativecommons.org/licenses/by/3.0/'): 'CC-BY-3.0-INTL',
    ('Attribution 1.0 Finland', 'http://creativecommons.org/licenses/by/1.0/fi/'): 'CC-BY-1.0-FI',
    ('Attribution 3.0 Netherlands', 'http://creativecommons.org/licenses/by/3.0/nl/'): 'CC-BY-3.0-NL',
    ('Attribution 2.0 UK: England', 'http://creativecommons.org/licenses/by/2.0/uk/'): 'CC-BY-2.0-UK',
    ('Attribution 2.0 France', 'http://creativecommons.org/licenses/by/2.0/fr/'): 'CC-BY-2.0-FR',
    ('Attribution 3.0 France', 'http://creativecommons.org/licenses/by/3.0/fr/'): 'CC-BY-3.0-FR',
}

def main():
    print("=== MULTI-DATASET PHYSICAL FILES, LABELS & INDEPENDENT SOURCE GROUPS ===")
    
    # 1. OpenMIC (Verified Commercial Only)
    test_keys = set(line.strip() for line in open(OPENMIC_TEST) if line.strip())
    train_keys = set(line.strip() for line in open(OPENMIC_TRAIN) if line.strip())
    
    openmic_train_tracks = []
    openmic_test_tracks = []
    
    with open(OPENMIC_META, 'r', encoding='utf-8', errors='ignore') as f:
        reader = csv.DictReader(f)
        for row in reader:
            skey = row.get('sample_key', '').strip()
            lt = row.get('license_title', '').strip()
            lu = row.get('license_url', '').strip()
            if (lt, lu) in VERIFIED_COMMERCIAL_ALLOWLIST:
                if skey in test_keys:
                    openmic_test_tracks.append((skey, row))
                elif skey in train_keys:
                    openmic_train_tracks.append((skey, row))
                    
    print(f"OpenMIC Verified Commercial: {len(openmic_train_tracks)} train tracks, {len(openmic_test_tracks)} locked test tracks")
    
    # 2. VocalSet
    # Official test singers: female2, female8, male3, male5, male10
    vocalset_test_singers = {'female2', 'female8', 'male3', 'male5', 'male10'}
    vocalset_val_singers = {'female7', 'female9', 'male11'}
    vocalset_train_singers = {'female1', 'female3', 'female4', 'female5', 'female6', 'male1', 'male2', 'male4', 'male6', 'male7', 'male8', 'male9'}
    
    vocal_files = {'train': [], 'val': [], 'test': []}
    for root, _, files in os.walk(os.path.join(VOCALSET_DIR, 'FULL')):
        for f in files:
            if f.endswith('.wav'):
                # Extract singer from path: e.g. Data/VocalSet/FULL/female1/arpeggios/...
                rel = os.path.relpath(root, os.path.join(VOCALSET_DIR, 'FULL'))
                parts = rel.split(os.sep)
                singer = parts[0]
                fp = os.path.join(root, f)
                if singer in vocalset_test_singers:
                    vocal_files['test'].append((fp, singer))
                elif singer in vocalset_val_singers:
                    vocal_files['val'].append((fp, singer))
                elif singer in vocalset_train_singers:
                    vocal_files['train'].append((fp, singer))
                    
    print(f"VocalSet (Solo Vocal): {len(vocal_files['train'])} train files (12 singers), {len(vocal_files['val'])} val files (3 singers), {len(vocal_files['test'])} test files (5 singers)")

    # 3. GuitarSet
    guitar_files = {'train': [], 'val': [], 'test': []}
    for root, _, files in os.walk(GUITARSET_DIR):
        for f in files:
            if f.endswith('.wav'):
                fp = os.path.join(root, f)
                player_id = f[:2]
                if player_id == '02':
                    guitar_files['test'].append((fp, player_id))
                elif player_id == '05':
                    guitar_files['val'].append((fp, player_id))
                else:
                    guitar_files['train'].append((fp, player_id))
                    
    print(f"GuitarSet (Solo Acoustic Guitar): {len(guitar_files['train'])} train files (4 players), {len(guitar_files['val'])} val files (1 player), {len(guitar_files['test'])} test files (1 player)")

    # 4. SYN-SE1 (Synthetic, kept separate)
    # Filename format: syn_se1_preset_01_01_C2.wav
    syn_files = {'train': [], 'val': [], 'test': []}
    for root, _, files in os.walk(SYNSE1_DIR):
        for f in files:
            if f.endswith('.wav') and 'preset_' in f:
                fp = os.path.join(root, f)
                # extract preset number
                idx = f.find('preset_')
                p_str = f[idx+7:idx+9]
                p_num = int(p_str) if p_str.isdigit() else 0
                if p_num >= 35:
                    syn_files['test'].append((fp, p_str))
                elif p_num >= 29:
                    syn_files['val'].append((fp, p_str))
                else:
                    syn_files['train'].append((fp, p_str))
    print(f"SYN-SE1 (Synthetic Synth Presets): {len(syn_files['train'])} train (28 presets), {len(syn_files['val'])} val (6 presets), {len(syn_files['test'])} test (6 presets)")

    # 5. Slakh2100 (Synthetic / Sequenced Stems, kept separate)
    # Tracks: Track00001 to Track00020
    # Test: Track00018 to Track00020 (3 songs)
    # Val: Track00015 to Track00017 (3 songs)
    # Train: Track00001 to Track00014 (14 songs)
    slakh_files = {'train': [], 'val': [], 'test': []}
    for root, _, files in os.walk(SLAKH_DIR):
        for f in files:
            if f.endswith('.wav'):
                fp = os.path.join(root, f)
                # track song folder
                parts = fp.split(os.sep)
                track_name = [p for p in parts if p.startswith('Track') or p.startswith('track')]
                t_id = track_name[0] if track_name else 'unknown'
                num = int(''.join(filter(str.isdigit, t_id))) if any(c.isdigit() for c in t_id) else 0
                if num >= 18:
                    slakh_files['test'].append((fp, t_id))
                elif num >= 15:
                    slakh_files['val'].append((fp, t_id))
                else:
                    slakh_files['train'].append((fp, t_id))
    print(f"Slakh2100 (Sequenced Multitrack Stems): {len(slakh_files['train'])} train (14 songs), {len(slakh_files['val'])} val (3 songs), {len(slakh_files['test'])} test (3 songs)")

if __name__ == '__main__':
    main()
