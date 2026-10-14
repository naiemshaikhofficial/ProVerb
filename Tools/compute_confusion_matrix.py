import json
import os
import numpy as np
import onnxruntime as ort

MANIFEST_PATH = "Assets/training_data_manifest.json"
MODEL_CANDIDATE_PATH = "Assets/ProVerb_AI_candidate.onnx"

with open(MANIFEST_PATH, "r", encoding="utf-8") as f:
    manifest = json.load(f)

recordings = manifest.get("recordings", [])

# Group recordings by parent_track_id
groups = {}
for r in recordings:
    pid = r.get("parent_track_id", "")
    groups.setdefault(pid, []).append(r)

# Deterministic hash split
import hashlib
train_groups, val_groups, test_groups = [], [], []
for gid in sorted(groups.keys()):
    h = int(hashlib.md5(gid.encode("utf-8")).hexdigest(), 16) % 100
    if h < 70:
        train_groups.append(gid)
    elif h < 85:
        val_groups.append(gid)
    else:
        test_groups.append(gid)

print("Test groups:", test_groups)

# Now inspect what happens during feature extraction for stems in test_groups
import scipy.signal
import soundfile as sf

# Feature extraction function exactly as train_proverb_model.py
def extract_features(mono, sr=16000):
    n_samples = len(mono)
    peak_level = float(np.max(np.abs(mono)))
    mean_rms = float(np.sqrt(np.mean(mono * mono)))
    if mean_rms < 0.002:
        return None, True

    abs_mono = np.abs(mono)
    w1 = 2.0 * np.pi * 95.0 / sr
    w2 = 2.0 * np.pi * 320.0 / sr
    w3 = 2.0 * np.pi * 1800.0 / sr
    w4 = 2.0 * np.pi * 6500.0 / sr

    a1 = np.exp(-w1); b1 = 1.0 - a1
    a2 = np.exp(-w2); b2 = 1.0 - a2
    a3 = np.exp(-w3); b3 = 1.0 - a3
    a4 = np.exp(-w4); b4 = 1.0 - a4

    fast_coeff = 1.0 - np.exp(-1.0 / (0.003 * sr))
    slow_coeff = 1.0 - np.exp(-1.0 / (0.045 * sr))
    refractory_samples = int(sr * 0.040)

    lp1 = scipy.signal.lfilter([b1], [1.0, -a1], mono)
    lp2 = scipy.signal.lfilter([b2], [1.0, -a2], mono)
    lp3 = scipy.signal.lfilter([b3], [1.0, -a3], mono)
    lp4 = scipy.signal.lfilter([b4], [1.0, -a4], mono)

    b_sub        = lp1
    b_bass       = lp2 - lp1
    b_vocal_core = lp3 - lp2
    b_vocal_pres = lp4 - lp3
    b_air        = mono - lp4

    sub_energy        = float(np.sum(b_sub * b_sub))
    bass_energy       = float(np.sum(b_bass * b_bass))
    vocal_core_energy = float(np.sum(b_vocal_core * b_vocal_core))
    vocal_pres_energy = float(np.sum(b_vocal_pres * b_vocal_pres))
    air_energy        = float(np.sum(b_air * b_air))

    total_band_energy = sub_energy + bass_energy + vocal_core_energy + vocal_pres_energy + air_energy + 1e-12

    crest_factor = (peak_level / mean_rms) if mean_rms > 1e-5 else 1.0
    first_crossing = 1 if (mono[0] < 0.0) else 0
    crossings = first_crossing + np.sum(((mono[:-1] >= 0.0) & (mono[1:] < 0.0)) | ((mono[:-1] < 0.0) & (mono[1:] >= 0.0)))
    zcr = float(crossings) / max(1, n_samples)

    env_fast = scipy.signal.lfilter([fast_coeff], [1.0, -(1.0 - fast_coeff)], abs_mono)
    env_slow = scipy.signal.lfilter([slow_coeff], [1.0, -(1.0 - slow_coeff)], abs_mono)
    env_diff = env_fast - env_slow
    env_dynamics = float(np.sum(env_diff * env_diff) * 10000.0) / max(1, n_samples)

    spike_candidates = (env_fast > (env_slow * 2.2 + 0.015)) & (abs_mono > 0.02)
    spike_indices = np.where(spike_candidates)[0]
    transient_hits = 0
    last_hit = -refractory_samples
    for idx in spike_indices:
        if idx - last_hit > refractory_samples:
            transient_hits += 1
            last_hit = idx

    duration_sec = n_samples / sr
    transient_density = float(transient_hits) / max(0.5, duration_sec)

    feats = np.array([
        np.clip(sub_energy / total_band_energy, 0.0, 1.0),
        np.clip(bass_energy / total_band_energy, 0.0, 1.0),
        np.clip(vocal_core_energy / total_band_energy, 0.0, 1.0),
        np.clip(vocal_pres_energy / total_band_energy, 0.0, 1.0),
        np.clip(air_energy / total_band_energy, 0.0, 1.0),
        np.clip(crest_factor, 1.0, 50.0),
        np.clip(zcr, 0.0, 1.0),
        np.clip(transient_density, 0.0, 30.0),
        np.clip(env_dynamics, 0.0, 100.0)
    ], dtype=np.float32)
    return feats, False

# Collect test chunk features
test_recs = [r for r in recordings if r.get("parent_track_id") in test_groups]
print(f"Test stems count: {len(test_recs)}")

sess = ort.InferenceSession(MODEL_CANDIDATE_PATH)
X_test, y_test = [], []

for r in test_recs:
    fpath = r["file_path"]
    cidx = r["class_index"]
    if not os.path.exists(fpath):
        continue
    data, sr = sf.read(fpath, dtype='float32')
    if data.ndim > 1:
        data = np.mean(data, axis=1)
    chunk_len = int(sr * 2.5)
    for start in range(0, len(data) - chunk_len + 1, chunk_len):
        chunk = data[start:start + chunk_len]
        feats, is_sil = extract_features(chunk, sr)
        if not is_sil:
            X_test.append(feats)
            y_test.append(cidx)

X_test = np.array(X_test, dtype=np.float32)
y_test = np.array(y_test, dtype=np.int64)
print(f"Extracted test chunks: {len(X_test)}")

preds = []
batch_size = 128
for i in range(0, len(X_test), batch_size):
    bx = X_test[i:i+batch_size]
    out = sess.run(None, {"input": bx})[0]
    preds.extend(np.argmax(out, axis=1))

preds = np.array(preds)

cm = np.zeros((7, 7), dtype=int)
for t, p in zip(y_test, preds):
    cm[t, p] += 1

CLASS_NAMES = ["Drums", "Vocals", "Guitar", "Piano", "Bass", "Synth", "Other"]
print("\n" + "="*60)
print("CONFUSION MATRIX ON HELD-OUT TEST DATA")
print("="*60)
header = f"{'Actual':<10} | " + " | ".join([f"{c[:6]:>6}" for c in CLASS_NAMES])
print(header)
print("-" * len(header))
for i in range(7):
    row = f"{CLASS_NAMES[i]:<10} | " + " | ".join([f"{cm[i, j]:6d}" for j in range(7)])
    print(row)

acc = np.mean(preds == y_test) * 100.0
print(f"\nRecomputed Test Accuracy: {acc:.2f}%")
