import os
import csv
import json
from collections import defaultdict, Counter

OPENMIC_META = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/openmic-2018-metadata.csv'
OPENMIC_TEST = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/partitions/split01_test.csv'
OPENMIC_RESPONSES = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/openmic-2018-aggregated-labels.csv'

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

# OpenMIC mapping to families
# OpenMIC 20 classes:
# 0: accordion, 1: banjo, 2: bass, 3: cello, 4: clarinet, 5: cymbals, 6: drums, 7: flute,
# 8: guitar, 9: mallet_percussion, 10: mandolin, 11: organ, 12: piano, 13: saxophone,
# 14: synthesizer, 15: trombone, 16: trumpet, 17: ukulele, 18: violin, 19: voice

def main():
    test_keys = set(line.strip() for line in open(OPENMIC_TEST) if line.strip())
    
    allow_test = {}
    with open(OPENMIC_META, 'r', encoding='utf-8', errors='ignore') as f:
        reader = csv.DictReader(f)
        for row in reader:
            skey = row.get('sample_key', '').strip()
            lt = row.get('license_title', '').strip()
            lu = row.get('license_url', '').strip()
            if (lt, lu) in VERIFIED_COMMERCIAL_ALLOWLIST and skey in test_keys:
                allow_test[skey] = row
                
    labels_by_sample = defaultdict(dict)
    with open(OPENMIC_RESPONSES, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        next(reader)
        for row in reader:
            if len(row) >= 3:
                skey = row[0].strip()
                inst = row[1].strip()
                rel = float(row[2])
                labels_by_sample[skey][inst] = rel

    # Map OpenMIC instruments to 8 Families
    inst_to_family = {
        'voice': '0: Voice',
        'drums': '1: Drums & Percussion',
        'cymbals': '1: Drums & Percussion',
        'bass': '2: Bass',
        'piano': '3: Keys & Mallets',
        'organ': '3: Keys & Mallets',
        'accordion': '3: Keys & Mallets',
        'mallet_percussion': '3: Keys & Mallets',
        'guitar': '4: Plucked Strings',
        'banjo': '4: Plucked Strings',
        'mandolin': '4: Plucked Strings',
        'ukulele': '4: Plucked Strings',
        'violin': '5: Bowed Strings',
        'cello': '5: Bowed Strings',
        'flute': '6: Winds & Brass',
        'clarinet': '6: Winds & Brass',
        'saxophone': '6: Winds & Brass',
        'trumpet': '6: Winds & Brass',
        'trombone': '6: Winds & Brass',
        'synthesizer': '7: Electronic'
    }

    fam_pos = defaultdict(int)
    fam_artists = defaultdict(set)
    fam_tracks = defaultdict(set)

    for skey, meta in allow_test.items():
        art = meta.get('artist_id') or meta.get('artist_name')
        labs = labels_by_sample.get(skey, {})
        for inst, rel in labs.items():
            if rel >= 0.5 and inst in inst_to_family:
                fam = inst_to_family[inst]
                fam_pos[fam] += 1
                fam_artists[fam].add(art)
                fam_tracks[fam].add(skey)

    print("=== TIER 1 REAL-RECORDINGS TEST COVERAGE BY 8 BROAD FAMILIES ===")
    print(f"{'Source Family':<25} | {'OpenMIC Pos (Tracks / Artists)':<35} | {'Dedicated Real Datasets Held-Out':<35}")
    print("-" * 105)
    
    print(f"{'0: Voice':<25} | {len(fam_tracks['0: Voice']):2d} tracks ({len(fam_artists['0: Voice']):2d} artists)               | + VocalSet: 914 files (5 held-out singers)")
    print(f"{'1: Drums & Percussion':<25} | {len(fam_tracks['1: Drums & Percussion']):2d} tracks ({len(fam_artists['1: Drums & Percussion']):2d} artists)               | None (Acoustic stems needed for high power)")
    print(f"{'2: Bass':<25} | {len(fam_tracks['2: Bass']):2d} tracks ({len(fam_artists['2: Bass']):2d} artists) [SPARSE]      | None (Real solo bass recordings needed)")
    print(f"{'3: Keys & Mallets':<25} | {len(fam_tracks['3: Keys & Mallets']):2d} tracks ({len(fam_artists['3: Keys & Mallets']):2d} artists)               | None (Solo piano/keys needed)")
    print(f"{'4: Plucked Strings':<25} | {len(fam_tracks['4: Plucked Strings']):2d} tracks ({len(fam_artists['4: Plucked Strings']):2d} artists)               | + GuitarSet: 60 files (Player 02)")
    print(f"{'5: Bowed Strings':<25} | {len(fam_tracks['5: Bowed Strings']):2d} tracks ({len(fam_artists['5: Bowed Strings']):2d} artists)               | None (Acoustic strings solo needed)")
    print(f"{'6: Winds & Brass':<25} | {len(fam_tracks['6: Winds & Brass']):2d} tracks ({len(fam_artists['6: Winds & Brass']):2d} artists)               | None (Acoustic winds solo needed)")
    print(f"{'7: Electronic':<25} | {len(fam_tracks['7: Electronic']):2d} tracks ({len(fam_artists['7: Electronic']):2d} artists)               | (TIER 2 ONLY: SYN-SE1 150 files, 6 presets)")

if __name__ == '__main__':
    main()
