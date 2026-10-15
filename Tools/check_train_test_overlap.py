import csv
from collections import defaultdict

METADATA_PATH = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/openmic-2018-metadata.csv'
TEST_SPLIT_PATH = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/partitions/split01_test.csv'
TRAIN_SPLIT_PATH = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/partitions/split01_train.csv'

# Strict explicit allowlists (URL and exact metadata title)
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
    test_keys = set(line.strip() for line in open(TEST_SPLIT_PATH) if line.strip())
    train_keys = set(line.strip() for line in open(TRAIN_SPLIT_PATH) if line.strip())

    train_meta = {}
    test_meta = {}

    with open(METADATA_PATH, 'r', encoding='utf-8', errors='ignore') as f:
        reader = csv.DictReader(f)
        for row in reader:
            skey = row.get('sample_key', '').strip()
            lt = row.get('license_title', '').strip()
            lu = row.get('license_url', '').strip()
            if (lt, lu) in VERIFIED_COMMERCIAL_ALLOWLIST:
                if skey in train_keys:
                    train_meta[skey] = row
                elif skey in test_keys:
                    test_meta[skey] = row

    print(f"Allowlisted Train Keys: {len(train_meta)}")
    print(f"Allowlisted Test Keys:  {len(test_meta)}")

    # 1. Track ID Overlap
    train_track_ids = set(r['track_id'] for r in train_meta.values())
    test_track_ids = set(r['track_id'] for r in test_meta.values())
    track_overlap = train_track_ids.intersection(test_track_ids)
    print(f"Track ID overlap: {len(track_overlap)}")

    # 2. Album ID Overlap
    train_album_ids = set(r['album_id'] for r in train_meta.values() if r['album_id'])
    test_album_ids = set(r['album_id'] for r in test_meta.values() if r['album_id'])
    album_overlap = train_album_ids.intersection(test_album_ids)
    print(f"Album ID overlap: {len(album_overlap)}")

    # 3. Artist ID Overlap
    train_artist_ids = set(r['artist_id'] for r in train_meta.values() if r['artist_id'])
    test_artist_ids = set(r['artist_id'] for r in test_meta.values() if r['artist_id'])
    artist_overlap = train_artist_ids.intersection(test_artist_ids)
    print(f"Artist ID overlap: {len(artist_overlap)}")

    if artist_overlap:
        print(f"Warning: {len(artist_overlap)} artists appear in both official train and test!")
        # Let's inspect tracks belonging to overlapping artists
        overlapping_test_keys = [k for k, r in test_meta.items() if r['artist_id'] in artist_overlap]
        print(f"Test tracks from overlapping artists: {len(overlapping_test_keys)} / {len(test_meta)}")

if __name__ == '__main__':
    main()
