import os
import csv
import json
from collections import defaultdict, Counter

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA_ROOT = os.path.join(PROJECT_ROOT, "Data")
OPENMIC_DIR = os.path.join(DATA_ROOT, "OpenMIC", "openmic-2018")
METADATA_PATH = os.path.join(OPENMIC_DIR, "openmic-2018-metadata.csv")
TEST_SPLIT_PATH = os.path.join(OPENMIC_DIR, "partitions", "split01_test.csv")
TRAIN_SPLIT_PATH = os.path.join(OPENMIC_DIR, "partitions", "split01_train.csv")
RESPONSES_PATH = os.path.join(OPENMIC_DIR, "openmic-2018-aggregated-labels.csv")

VOCALSET_DIR = os.path.join(DATA_ROOT, "VocalSet")
GUITARSET_DIR = os.path.join(DATA_ROOT, "GuitarSet")
SYNSE1_DIR = os.path.join(DATA_ROOT, "SYN-SE1")
SLAKH_DIR = os.path.join(DATA_ROOT, "Slakh")

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

PUBLIC_DOMAIN_REVIEW_ALLOWLIST = {
    ('Public Domain Mark 1.0', 'http://creativecommons.org/publicdomain/mark/1.0/'): 'PDM-1.0-REVIEW',
    ('Public Domain', 'http://creativecommons.org/licenses/publicdomain/'): 'PD-UNSPECIFIED-REVIEW',
}

OPENMIC_FAMILY_MAP = {
    0: ['voice'],
    1: ['drums', 'cymbals'],
    2: ['bass'],
    3: ['piano', 'organ', 'accordion', 'mallet_percussion'],
    4: ['guitar', 'banjo', 'mandolin', 'ukulele'],
    5: ['violin', 'cello'],
    6: ['flute', 'clarinet', 'saxophone', 'trumpet', 'trombone'],
    7: ['synthesizer']
}

def load_openmic_labels():
    labels = defaultdict(dict)
    with open(RESPONSES_PATH, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        next(reader)
        for row in reader:
            if len(row) >= 3:
                skey = row[0].strip()
                inst = row[1].strip()
                rel = float(row[2])
                labels[skey][inst] = rel
    return labels

def main():
    print("Generating comprehensive dataset manifest...")
    openmic_labels = load_openmic_labels()

    test_keys = set(line.strip() for line in open(TEST_SPLIT_PATH) if line.strip())
    train_keys = set(line.strip() for line in open(TRAIN_SPLIT_PATH) if line.strip())

    manifest = []
    pdm_review_manifest = []

    # 1. Process OpenMIC
    with open(METADATA_PATH, 'r', encoding='utf-8', errors='ignore') as f:
        reader = csv.DictReader(f)
        for row in reader:
            skey = row.get('sample_key', '').strip()
            lt = row.get('license_title', '').strip()
            lu = row.get('license_url', '').strip()
            pair = (lt, lu)

            audio_fp = os.path.join(OPENMIC_DIR, "audio", skey[:3], f"{skey}.ogg")
            if not os.path.exists(audio_fp):
                continue

            # Determine split
            if skey in test_keys:
                split = "test"
            elif skey in train_keys:
                split = "train"
            else:
                continue

            # Multi-label target & mask
            y_target = [0.0] * 8
            y_mask = [False] * 8
            s_labs = openmic_labels.get(skey, {})

            for fam_idx, inst_list in OPENMIC_FAMILY_MAP.items():
                eval_instances = [s_labs[inst] for inst in inst_list if inst in s_labs]
                if eval_instances:
                    y_mask[fam_idx] = True
                    if any(rel >= 0.5 for rel in eval_instances):
                        y_target[fam_idx] = 1.0

            entry = {
                "id": skey,
                "dataset": "OpenMIC-2018",
                "tier": "real_acoustic",
                "split": split,
                "file_path": audio_fp,
                "parent_group": row.get('artist_id') or row.get('artist_name') or "unknown_artist",
                "artist_name": row.get('artist_name', ''),
                "track_title": row.get('track_title', ''),
                "track_url": row.get('track_url', ''),
                "license_category": "",
                "license_title": lt,
                "license_url": lu,
                "target": y_target,
                "mask": y_mask
            }

            if pair in VERIFIED_COMMERCIAL_ALLOWLIST:
                entry["license_category"] = VERIFIED_COMMERCIAL_ALLOWLIST[pair]
                manifest.append(entry)
            elif pair in PUBLIC_DOMAIN_REVIEW_ALLOWLIST:
                entry["license_category"] = PUBLIC_DOMAIN_REVIEW_ALLOWLIST[pair]
                pdm_review_manifest.append(entry)

    # Further partition OpenMIC train into train and val (disjoint by artist!)
    # Deterministic artist-level split on train tracks
    train_artists = set(e['parent_group'] for e in manifest if e['split'] == 'train')
    sorted_train_artists = sorted(list(train_artists))
    # Reserve ~20% of train artists for validation
    val_artists = set(sorted_train_artists[::5])
    for e in manifest:
        if e['split'] == 'train' and e['parent_group'] in val_artists:
            e['split'] = 'val'

    # 2. VocalSet (Solo Voice)
    vocalset_test = {'female2', 'female8', 'male3', 'male5', 'male10'}
    vocalset_val = {'female7', 'female9', 'male11'}
    for root, _, files in os.walk(os.path.join(VOCALSET_DIR, "FULL")):
        for f in files:
            if f.endswith('.wav'):
                rel = os.path.relpath(root, os.path.join(VOCALSET_DIR, "FULL"))
                singer = rel.split(os.sep)[0]
                fp = os.path.join(root, f)
                
                if singer in vocalset_test:
                    split = "test"
                elif singer in vocalset_val:
                    split = "val"
                else:
                    split = "train"

                y_target = [0.0] * 8
                y_target[0] = 1.0  # Voice
                y_mask = [True] * 8 # Fully annotated solo take

                manifest.append({
                    "id": f"vocalset_{singer}_{f}",
                    "dataset": "VocalSet",
                    "tier": "real_acoustic",
                    "split": split,
                    "file_path": fp,
                    "parent_group": singer,
                    "artist_name": f"Singer {singer}",
                    "track_title": f,
                    "track_url": "https://doi.org/10.5281/zenodo.1193957",
                    "license_category": "CC-BY-4.0",
                    "license_title": "Creative Commons Attribution 4.0 International",
                    "license_url": "https://creativecommons.org/licenses/by/4.0/",
                    "target": y_target,
                    "mask": y_mask
                })

    # 3. GuitarSet (Solo Acoustic Guitar)
    for root, _, files in os.walk(GUITARSET_DIR):
        for f in files:
            if f.endswith('.wav'):
                player = f[:2]
                fp = os.path.join(root, f)
                if player == '02':
                    split = "test"
                elif player == '05':
                    split = "val"
                else:
                    split = "train"

                y_target = [0.0] * 8
                y_target[4] = 1.0  # Plucked Strings
                y_mask = [True] * 8

                manifest.append({
                    "id": f"guitarset_{f}",
                    "dataset": "GuitarSet",
                    "tier": "real_acoustic",
                    "split": split,
                    "file_path": fp,
                    "parent_group": f"player_{player}",
                    "artist_name": f"Guitarist {player}",
                    "track_title": f,
                    "track_url": "https://doi.org/10.5281/zenodo.3371780",
                    "license_category": "CC-BY-4.0",
                    "license_title": "Creative Commons Attribution 4.0 International",
                    "license_url": "https://creativecommons.org/licenses/by/4.0/",
                    "target": y_target,
                    "mask": y_mask
                })

    # 4. SYN-SE1 (Synthetic Synth Presets)
    for root, _, files in os.walk(SYNSE1_DIR):
        for f in files:
            if f.endswith('.wav') and 'preset_' in f:
                idx = f.find('preset_')
                p_str = f[idx+7:idx+9]
                p_num = int(p_str) if p_str.isdigit() else 0
                fp = os.path.join(root, f)

                if p_num >= 35:
                    split = "test"
                elif p_num >= 29:
                    split = "val"
                else:
                    split = "train"

                y_target = [0.0] * 8
                y_target[7] = 1.0  # Electronic
                y_mask = [True] * 8

                manifest.append({
                    "id": f"synse1_{f}",
                    "dataset": "SYN-SE1",
                    "tier": "synthetic",
                    "split": split,
                    "file_path": fp,
                    "parent_group": f"preset_{p_str}",
                    "artist_name": "Synthesizer Sound Engine",
                    "track_title": f,
                    "track_url": "https://zenodo.org/record/4643666",
                    "license_category": "CC-BY-4.0",
                    "license_title": "Creative Commons Attribution 4.0 International",
                    "license_url": "https://creativecommons.org/licenses/by/4.0/",
                    "target": y_target,
                    "mask": y_mask
                })

    # 5. Slakh2100 (Synthetic Sequenced Stems)
    for root, _, files in os.walk(SLAKH_DIR):
        for f in files:
            if f.endswith('.wav'):
                fp = os.path.join(root, f)
                parts = fp.split(os.sep)
                track_name = [p for p in parts if p.startswith('Track') or p.startswith('track')]
                t_id = track_name[0] if track_name else 'unknown'
                num = int(''.join(filter(str.isdigit, t_id))) if any(c.isdigit() for c in t_id) else 0

                if num >= 18:
                    split = "test"
                elif num >= 15:
                    split = "val"
                else:
                    split = "train"

                # Stem instrument classification from filename
                f_low = f.lower()
                y_target = [0.0] * 8
                y_mask = [True] * 8

                if 'drum' in f_low:
                    y_target[1] = 1.0
                elif 'bass' in f_low:
                    y_target[2] = 1.0
                elif 'piano' in f_low or 'key' in f_low:
                    y_target[3] = 1.0
                elif 'guitar' in f_low:
                    y_target[4] = 1.0
                elif 'string' in f_low:
                    y_target[5] = 1.0
                else:
                    continue  # Only keep clearly identifiable stems

                manifest.append({
                    "id": f"slakh_{t_id}_{f}",
                    "dataset": "Slakh2100",
                    "tier": "synthetic",
                    "split": split,
                    "file_path": fp,
                    "parent_group": t_id,
                    "artist_name": f"Slakh Project {t_id}",
                    "track_title": f,
                    "track_url": "http://www.slakh.com",
                    "license_category": "CC-BY-4.0",
                    "license_title": "Creative Commons Attribution 4.0 International",
                    "license_url": "https://creativecommons.org/licenses/by/4.0/",
                    "target": y_target,
                    "mask": y_mask
                })

    # Write manifests
    out_manifest_path = os.path.join(DATA_ROOT, "manifest.json")
    with open(out_manifest_path, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)

    out_pdm_path = os.path.join(DATA_ROOT, "pdm_review_manifest.json")
    with open(out_pdm_path, 'w', encoding='utf-8') as f:
        json.dump(pdm_review_manifest, f, indent=2)

    print(f"\nSuccessfully generated {out_manifest_path} with {len(manifest)} verified commercial items.")
    print(f"Generated {out_pdm_path} with {len(pdm_review_manifest)} Public Domain review items.")

    # Split summary
    split_counts = Counter(e['split'] for e in manifest)
    tier_counts = Counter((e['dataset'], e['tier'], e['split']) for e in manifest)
    print("\nManifest Split Summary:")
    for k, v in sorted(split_counts.items()):
        print(f"  {k:<10}: {v:5d} items")
    print("\nDataset Breakdown:")
    for (ds, tier, sp), count in sorted(tier_counts.items()):
        print(f"  {ds:<15} [{tier:<13}] {sp:<8}: {count:5d} items")

if __name__ == '__main__':
    main()
