#!/usr/bin/env python3
"""
MUSDB18 Research Adapter (Non-Commercial / Disabled by Default)
ProducerToy — ProVerb Smart Algorithmic Reverb

Official Source: https://zenodo.org/records/1117372
SigSep / Inria / Zenodo

LEGAL NOTICE & COMMERCIAL EXCLUSION:
MUSDB18 contains stems of full songs (vocals, drums, bass, other) provided
strictly for academic research purposes. The rights holders have NOT granted
a general commercial license.

PROVERB POLICY:
MUSDB18 is EXCLUDED by default from the commercial ProVerb training pipeline.
This adapter exists solely for non-commercial research comparison and requires
the explicit flag: --research-only-non-commercial.
Any model trained using this adapter is legally classified as NON-COMMERCIAL RESEARCH ONLY
and MUST NOT be deployed into the commercial ProVerb VST3 / Standalone distribution.
"""

import os
import sys

DATASET_NAME = "MUSDB18"
SOURCE_URL = "https://zenodo.org/records/1117372"
LICENSE_RESTRICTION = "NON-COMMERCIAL RESEARCH ONLY (Express rights holder permission required for commercial products)"


def check_musdb_commercial_permission(allow_research_only: bool = False) -> bool:
    if not allow_research_only:
        print("\n" + "!" * 78)
        print(" [LEGAL RESTRICTION] MUSDB18 IS EXCLUDED FROM THE COMMERCIAL PIPELINE")
        print(" MUSDB18 terms restrict commercial use without express rights holder permission.")
        print(" ProducerToy ProVerb is a commercial product and cannot use MUSDB18 by default.")
        print(" To run an isolated academic benchmark, pass: --research-only-non-commercial")
        print("!" * 78 + "\n")
        return False
    return True


if __name__ == "__main__":
    allow_research = "--research-only-non-commercial" in sys.argv
    if not check_musdb_commercial_permission(allow_research):
        sys.exit(1)
    print("[*] MUSDB18 research adapter initialized in NON-COMMERCIAL mode.")
