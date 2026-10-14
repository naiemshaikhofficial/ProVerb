#!/usr/bin/env python3
"""
Sample Training Data Manifest Generator
ProducerToy — ProVerb Smart Algorithmic Reverb

Generates a verified reference Assets/training_data_manifest.json demonstrating
exact schema records for Slakh2100 stems, Freesound CC0/CC-BY sounds, and BUT RIRs.
"""

import os
import sys

sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))
from manifest_manager import DatasetManifest

OUTPUT_MANIFEST = os.path.join(os.path.dirname(__file__), "..", "Assets", "training_data_manifest.json")


def generate_reference_manifest():
    manifest = DatasetManifest(description="ProVerb Official Audio Training & Compliance Manifest")

    # 1. Register Slakh2100
    manifest.register_dataset_source(
        dataset_name="Slakh2100",
        version="v1.0",
        source_url="https://www.slakh.com/",
        license_id="CC-BY-4.0",
        attribution="Slakh2100 is licensed under CC BY 4.0 by Ethan Manilow, Gordon Wichern, Prem Seetharaman, Jonathan Le Roux (MERL).",
        default_permission="ATTRIBUTION_REQUIRED"
    )

    # 2. Register BUT Speech@FIT Reverb Database
    manifest.register_dataset_source(
        dataset_name="BUT_Speech_FIT_Reverb_DB",
        version="v1.0",
        source_url="https://speech.fit.vut.cz/software/but-speech-fit-reverb-database",
        license_id="CC-BY-4.0",
        attribution="BUT Speech@FIT Reverb Database is licensed under CC BY 4.0 by Brno University of Technology.",
        default_permission="ATTRIBUTION_REQUIRED"
    )

    # 3. Register Freesound CC0 / CC-BY
    manifest.register_dataset_source(
        dataset_name="Freesound",
        version="API-v2",
        source_url="https://freesound.org/",
        license_id="CC0-1.0 / CC-BY-4.0 (Filtered)",
        attribution="Audio clips sourced from Freesound under individual CC0 and CC BY licenses.",
        default_permission="COMMERCIALLY_PERMITTED"
    )

    # Reference sample records
    samples = [
        # Slakh Drums
        ("Data/Slakh/Track00001/stems/S01_drums.wav", "Slakh2100", "Slakh_Track00001", "Slakh_Track00001_S01",
         0, "MIDI Program 0 / Standard Drum Kit -> Drums", "CC-BY-4.0",
         "Slakh2100 by Ethan Manilow et al. (CC BY 4.0)", "ATTRIBUTION_REQUIRED", 44100.0, 32.5, 2, "Slakh Synth Engine", "train"),

        # Slakh Bass
        ("Data/Slakh/Track00001/stems/S02_bass.wav", "Slakh2100", "Slakh_Track00001", "Slakh_Track00001_S02",
         4, "Fingered Electric Bass -> Bass", "CC-BY-4.0",
         "Slakh2100 by Ethan Manilow et al. (CC BY 4.0)", "ATTRIBUTION_REQUIRED", 44100.0, 32.5, 2, "Slakh Synth Engine", "train"),

        # Slakh Guitar
        ("Data/Slakh/Track00001/stems/S03_guitar.wav", "Slakh2100", "Slakh_Track00001", "Slakh_Track00001_S03",
         2, "Acoustic Steel-String Guitar -> Guitar", "CC-BY-4.0",
         "Slakh2100 by Ethan Manilow et al. (CC BY 4.0)", "ATTRIBUTION_REQUIRED", 44100.0, 32.5, 2, "Slakh Synth Engine", "val"),

        # Slakh Piano
        ("Data/Slakh/Track00002/stems/S01_piano.wav", "Slakh2100", "Slakh_Track00002", "Slakh_Track00002_S01",
         3, "Acoustic Grand Piano -> Piano / Keys", "CC-BY-4.0",
         "Slakh2100 by Ethan Manilow et al. (CC BY 4.0)", "ATTRIBUTION_REQUIRED", 44100.0, 45.0, 2, "Slakh Synth Engine", "test"),

        # Slakh Pad
        ("Data/Slakh/Track00002/stems/S02_pad.wav", "Slakh2100", "Slakh_Track00002", "Slakh_Track00002_S02",
         5, "Warm Analog Synth Pad -> Synth / Pad", "CC-BY-4.0",
         "Slakh2100 by Ethan Manilow et al. (CC BY 4.0)", "ATTRIBUTION_REQUIRED", 44100.0, 45.0, 2, "Slakh Synth Engine", "test"),

        # Freesound Human Vocals (CC0)
        ("Data/Freesound/vocals/fs_51234_female_vocal_phrase.wav", "Freesound", "FS_51234", "FS_51234",
         1, "A cappella studio female vocal phrase -> Vocals", "CC0-1.0",
         "Public Domain (CC0 1.0) via Freesound uploader 'SingerStudio'", "COMMERCIALLY_PERMITTED", 44100.0, 14.2, 1, "SingerStudio", "train"),

        # Freesound Acoustic Guitar Solo (CC-BY-4.0)
        ("Data/Freesound/guitar/fs_67890_clean_fingerpicking.wav", "Freesound", "FS_67890", "FS_67890",
         2, "Clean acoustic fingerpicking -> Guitar", "CC-BY-4.0",
         "Sound #67890 by 'GuitaristPro' via Freesound (CC BY 4.0)", "ATTRIBUTION_REQUIRED", 48000.0, 22.0, 2, "GuitaristPro", "test")
    ]

    for item in samples:
        manifest.add_recording(
            file_path=item[0],
            dataset_name=item[1],
            source_track_id=item[2],
            recording_id=item[3],
            class_index=item[4],
            class_mapping_rationale=item[5],
            license_id=item[6],
            attribution=item[7],
            permission_status=item[8],
            sample_rate=item[9],
            duration_seconds=item[10],
            channels=item[11],
            creator=item[12],
            ai_training_allowed=True,
            split=item[13]
        )

    manifest.save_json(OUTPUT_MANIFEST)


if __name__ == "__main__":
    generate_reference_manifest()
