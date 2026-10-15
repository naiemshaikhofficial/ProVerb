"""Offline ProVerb license issuer launcher.

Usage:
  python Tools/generate_proverb_license_file.py --device-id "<copied device ID>" \
      --name "Customer Name" --email "customer@example.com" [--output license.ptlic]
"""
from pathlib import Path
import shutil
import subprocess
import sys

node = shutil.which("node")
if node is None:
    raise SystemExit("Node.js is required for Ed25519 license signing. Install Node.js, then retry.")

issuer = Path(__file__).with_name("issue_proverb_license.mjs")
if not issuer.is_file():
    raise SystemExit(f"Offline issuer is missing: {issuer}")

completed = subprocess.run([node, str(issuer), *sys.argv[1:]], check=False)
raise SystemExit(completed.returncode)
