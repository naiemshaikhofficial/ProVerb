import csv
import os

METADATA_PATH = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/openmic-2018-metadata.csv'
AUDIO_DIR = 'd:/Website/ProducerToy-Plugins/ProVerb/Data/OpenMIC/openmic-2018/audio'

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

PUBLIC_DOMAIN_REVIEW_ALLOWLIST = {
    ('Public Domain Mark 1.0', 'http://creativecommons.org/publicdomain/mark/1.0/'): 'PDM-1.0-REVIEW',
    ('Public Domain', 'http://creativecommons.org/licenses/publicdomain/'): 'PD-UNSPECIFIED-REVIEW',
}

def get_audio_path(sample_key):
    # In OpenMIC, files are organized as audio/000/000046_3840.ogg
    prefix = sample_key[:3]
    return os.path.join(AUDIO_DIR, prefix, f"{sample_key}.ogg")

def main():
    verified_comm_tracks = []
    pd_review_tracks = []
    quarantined_tracks = []
    
    missing_files = []

    with open(METADATA_PATH, 'r', encoding='utf-8', errors='ignore') as f:
        reader = csv.DictReader(f)
        for row in reader:
            skey = row.get('sample_key', '').strip()
            lt = row.get('license_title', '').strip()
            lu = row.get('license_url', '').strip()
            pair = (lt, lu)
            
            audio_fp = get_audio_path(skey)
            exists = os.path.exists(audio_fp)
            if not exists:
                missing_files.append((skey, audio_fp))

            item = {
                'sample_key': skey,
                'track_id': row.get('track_id'),
                'artist_id': row.get('artist_id'),
                'artist_name': row.get('artist_name'),
                'artist_url': row.get('artist_url'),
                'album_id': row.get('album_id'),
                'album_title': row.get('album_title'),
                'track_title': row.get('track_title'),
                'track_url': row.get('track_url'),
                'license_title': lt,
                'license_url': lu,
                'audio_path': audio_fp,
                'file_exists': exists
            }

            if pair in VERIFIED_COMMERCIAL_ALLOWLIST:
                item['license_category'] = VERIFIED_COMMERCIAL_ALLOWLIST[pair]
                verified_comm_tracks.append(item)
            elif pair in PUBLIC_DOMAIN_REVIEW_ALLOWLIST:
                item['license_category'] = PUBLIC_DOMAIN_REVIEW_ALLOWLIST[pair]
                pd_review_tracks.append(item)
            else:
                item['license_category'] = 'QUARANTINED_INELIGIBLE'
                quarantined_tracks.append(item)

    print("=== OPENMIC REPRODUCIBLE LICENSE & DISK AUDIT ===")
    print(f"Total rows in metadata: {len(verified_comm_tracks) + len(pd_review_tracks) + len(quarantined_tracks)}")
    print(f"Total missing audio files across entire 20,000 dataset: {len(missing_files)}")
    print(f"\n1. Verified Direct Commercial Grants (CC0 + plain CC-BY): {len(verified_comm_tracks)}")
    print(f"   - Physical files present on disk: {sum(1 for t in verified_comm_tracks if t['file_exists'])} / {len(verified_comm_tracks)}")
    print(f"\n2. Public Domain Review Bucket (PDM 1.0 + PD Unspecified): {len(pd_review_tracks)}")
    print(f"   - PDM 1.0: {sum(1 for t in pd_review_tracks if t['license_category'] == 'PDM-1.0-REVIEW')}")
    print(f"   - PD Unspecified: {sum(1 for t in pd_review_tracks if t['license_category'] == 'PD-UNSPECIFIED-REVIEW')}")
    print(f"   - Physical files present on disk: {sum(1 for t in pd_review_tracks if t['file_exists'])} / {len(pd_review_tracks)}")
    print(f"\n3. Quarantined Ineligible Tracks (NC, ND, SA, Sampling, Unknown, Blank): {len(quarantined_tracks)}")
    print(f"   - Physical files present on disk (kept on disk, excluded from training): {sum(1 for t in quarantined_tracks if t['file_exists'])} / {len(quarantined_tracks)}")

if __name__ == '__main__':
    main()
