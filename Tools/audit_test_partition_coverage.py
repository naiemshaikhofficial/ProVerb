import csv
import numpy as np
from collections import defaultdict, Counter

METADATA_PATH = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/openmic-2018-metadata.csv'
TEST_SPLIT_PATH = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/partitions/split01_test.csv'
TRAIN_SPLIT_PATH = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/partitions/split01_train.csv'
RESPONSES_PATH = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/openmic-2018-aggregated-labels.csv'

# Exact strict commercial allowlist
DISQUALIFIERS = ['-nc', 'noncommercial', 'non-commercial', '-nd', 'noderiv', 'no derivative', '-sa', 'sharealike', 'share-alike', 'share alike', 'sampling']

def load_allowlisted_tracks():
    allowlisted = {}
    with open(METADATA_PATH, 'r', encoding='utf-8', errors='ignore') as f:
        reader = csv.DictReader(f)
        for row in reader:
            tid = row.get('sample_key', '').strip()
            if not tid:
                tid = f"{int(row['track_id']):06d}_{int(float(row['start_time'])*1000)}"
            lt = row.get('license_title', '').strip()
            lu = row.get('license_url', '').strip()
            
            t_low = lt.lower()
            u_low = lu.lower()
            if not any(d in t_low or d in u_low for d in DISQUALIFIERS):
                allowlisted[tid] = {
                    'track_id': row.get('track_id'),
                    'artist_id': row.get('artist_id'),
                    'artist_name': row.get('artist_name'),
                    'album_id': row.get('album_id'),
                    'license_title': lt,
                    'license_url': lu,
                    'sample_key': tid
                }
    return allowlisted

def main():
    allowlisted = load_allowlisted_tracks()
    print(f"Total allowlisted tracks: {len(allowlisted)}")

    with open(TEST_SPLIT_PATH, 'r', encoding='utf-8') as f:
        test_keys = set(line.strip() for line in f if line.strip())

    with open(TRAIN_SPLIT_PATH, 'r', encoding='utf-8') as f:
        train_keys = set(line.strip() for line in f if line.strip())

    allow_test = {k: v for k, v in allowlisted.items() if k in test_keys}
    allow_train = {k: v for k, v in allowlisted.items() if k in train_keys}

    print(f"Official Test tracks allowlisted: {len(allow_test)} (out of {len(test_keys)})")
    print(f"Official Train tracks allowlisted: {len(allow_train)} (out of {len(train_keys)})")

    # Load aggregated labels
    # Format of aggregated labels
    with open(RESPONSES_PATH, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        header = next(reader)
        print("Labels header:", header)
        
        # sample_key, instrument, relevance, count
        # or pivot table
        labels_by_sample = defaultdict(dict)
        for row in reader:
            if len(row) >= 3:
                skey = row[0].strip()
                inst = row[1].strip()
                rel = float(row[2])
                labels_by_sample[skey][inst] = rel

    # Compute coverage for test partition
    classes = sorted(list(next(iter(labels_by_sample.values())).keys())) if labels_by_sample else []
    print(f"Found {len(classes)} instrument classes: {classes}")

    print("\n=== TEST PARTITION: CLASS POSITIVES & UNIQUE ARTIST/PARENT GROUPS ===")
    test_artists_per_class = defaultdict(set)
    test_pos_per_class = defaultdict(int)
    test_eval_per_class = defaultdict(int)

    for skey, meta in allow_test.items():
        art_id = meta.get('artist_id') or meta.get('artist_name')
        labs = labels_by_sample.get(skey, {})
        for inst, rel in labs.items():
            test_eval_per_class[inst] += 1
            if rel >= 0.5:
                test_pos_per_class[inst] += 1
                test_artists_per_class[inst].add(art_id)

    print(f"{'Class':<20} | {'Evaluated':<10} | {'Positives':<10} | {'Unique Artists':<15} | {'Feasibility / Sparsity Flag'}")
    print("-" * 80)
    for inst in sorted(test_eval_per_class.keys()):
        ev = test_eval_per_class[inst]
        pos = test_pos_per_class[inst]
        n_art = len(test_artists_per_class[inst])
        flag = "ADEQUATE"
        if pos < 15:
            flag = "CRITICALLY SPARSE (<15 pos) - Cannot support 90% CI lower bound!"
        elif pos < 25:
            flag = "MARGINAL (<25 pos)"
        print(f"{inst:<20} | {ev:<10} | {pos:<10} | {n_art:<15} | {flag}")

if __name__ == '__main__':
    main()
