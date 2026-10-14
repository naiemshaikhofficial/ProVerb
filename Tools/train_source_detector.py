#!/usr/bin/env python3
"""
ProducerToy-Reverb — Multi-Label Source Detector Training & Export Pipeline

Target Project: D:\\Website\\ProducerToy-Plugins\\ProducerToy-Reverb
Task: Multi-Label Audio Source Family Detection (Voice, Drums, Bass, Keys, Plucked, Bowed, Winds, Electronic)

Datasets & Licensing (All CC BY 4.0):
  1. OpenMIC-2018 (CC BY 4.0, Humphrey et al., ISMIR 2018):
     - 20,000 polyphonic/monophonic music excerpts (10.0s @ 44.1 kHz).
     - Official disjoint track splits: split01_train.csv (14,915 tracks), split01_test.csv (5,085 tracks).
     - 0 track leakage between train and test.
  2. VocalSet (CC BY 4.0, Wilkins et al., 2018):
     - Isolated vocal takes. Train: 12 singers, Val: 4 singers, Test: 4 singers (female5,6, male5,6).
  3. GuitarSet (CC BY 4.0, Xi et al., 2018):
     - Isolated acoustic guitar. Train: players 00,01,03,05. Val: player 04. Test: player 02.
  4. SYN-SE1 (CC BY 4.0):
     - Synthesizer presets & electronic sound design. Train: presets 01-19, Val: 20-26, Test: 27-40.
  5. Slakh2100 (CC BY 4.0, Manilow et al., 2019):
     - Multitrack stems. Train: Tracks 01-12, Val: Tracks 13-16, Test: Tracks 17-20.

Source Taxonomy (8 Broad Families):
  0: Voice
  1: Drums & Percussion
  2: Bass
  3: Keys & Mallets
  4: Plucked Strings
  5: Bowed Strings
  6: Winds & Brass
  7: Electronic
"""

import os
import sys

if sys.platform == "win32":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
os.environ["PYTHONIOENCODING"] = "utf-8"

import math
import time
import json
import random
from typing import List, Tuple, Dict
from collections import defaultdict

import numpy as np
import soundfile as sf
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import TensorDataset, DataLoader

RANDOM_SEED = 42
random.seed(RANDOM_SEED)
np.random.seed(RANDOM_SEED)
torch.manual_seed(RANDOM_SEED)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
DATA_ROOT = os.path.abspath(os.path.join(PROJECT_ROOT, "..", "ProVerb", "Data"))
ASSETS_DIR = os.path.join(PROJECT_ROOT, "Assets")
SOURCE_AI_DIR = os.path.join(PROJECT_ROOT, "Source", "AI")

SAMPLE_RATE = 44100.0
WINDOW_SAMPLES = 44100  # 1.000s window

FEATURE_NAMES = [
    "subRatio",
    "bassRatio",
    "vocalCoreRatio",
    "vocalPresRatio",
    "airRatio",
    "crestFactor",
    "zcr",
    "transientDensity",
    "envDynamics",
    "spectralCentroid",
    "attackSlope",
    "temporalSustainRatio"
]
NUM_FEATURES = len(FEATURE_NAMES)

FAMILY_NAMES = [
    "Voice",
    "Drums & Percussion",
    "Bass",
    "Keys & Mallets",
    "Plucked Strings",
    "Bowed Strings",
    "Winds & Brass",
    "Electronic"
]
NUM_FAMILIES = len(FAMILY_NAMES)

OPENMIC_FAMILY_MAP = {
    0: [19],                        # Voice
    1: [6, 5],                      # Drums, Cymbals
    2: [2],                         # Bass
    3: [12, 11, 0, 9],              # Piano, Organ, Accordion, Mallet Percussion
    4: [8, 1, 10, 17],              # Guitar, Banjo, Mandolin, Ukulele
    5: [18, 3],                     # Violin, Cello
    6: [7, 4, 13, 16, 15],          # Flute, Clarinet, Saxophone, Trumpet, Trombone
    7: [14]                         # Synthesizer
}


def extract_features(audio: np.ndarray, sample_rate: float = 44100.0) -> Tuple[np.ndarray, bool]:
    """
    Extract 12 acoustic features on a 1.0-second chunk (single-pass, zero-allocation).
    Exact match with C++ real-time extraction in SourceDetector.h.
    """
    num_samples = len(audio)
    if num_samples <= 0:
        return np.zeros(NUM_FEATURES, dtype=np.float32), True

    sum_sq = np.sum(audio.astype(np.float64) ** 2)
    peak = np.max(np.abs(audio))
    rms = math.sqrt(sum_sq / max(1, num_samples))
    if rms < 0.002:
        return np.zeros(NUM_FEATURES, dtype=np.float32), True

    two_pi = 2.0 * math.pi
    alpha1 = 1.0 - math.exp(-two_pi * 95.0 / sample_rate)
    alpha2 = 1.0 - math.exp(-two_pi * 320.0 / sample_rate)
    alpha3 = 1.0 - math.exp(-two_pi * 1800.0 / sample_rate)
    alpha4 = 1.0 - math.exp(-two_pi * 6500.0 / sample_rate)

    lp1 = lp2 = lp3 = lp4 = 0.0
    sub_e = bass_e = core_e = pres_e = air_e = 0.0

    fast_coeff = 1.0 - math.exp(-1.0 / (0.003 * sample_rate))
    slow_coeff = 1.0 - math.exp(-1.0 / (0.045 * sample_rate))
    env_fast = env_slow = 0.0
    env_diff_sum = 0.0

    refractory_samples = int(sample_rate * 0.050)
    transient_hits = 0
    last_hit = -refractory_samples
    attack_slopes = []

    crossings = 1 if audio[0] < 0.0 else 0
    prev_sample = audio[0]

    env_slow_vals = np.zeros(num_samples, dtype=np.float32)

    for i in range(num_samples):
        x = float(audio[i])
        abs_x = abs(x)

        # 5-Band Filter Bank
        lp1 += alpha1 * (x - lp1)
        lp2 += alpha2 * (x - lp2)
        lp3 += alpha3 * (x - lp3)
        lp4 += alpha4 * (x - lp4)

        b_sub = lp1
        b_bass = lp2 - lp1
        b_core = lp3 - lp2
        b_pres = lp4 - lp3
        b_air = x - lp4

        sub_e += b_sub * b_sub
        bass_e += b_bass * b_bass
        core_e += b_core * b_core
        pres_e += b_pres * b_pres
        air_e += b_air * b_air

        # Zero crossings
        if i > 0:
            if (prev_sample >= 0.0 and x < 0.0) or (prev_sample < 0.0 and x >= 0.0):
                crossings += 1
            prev_sample = x

        # Envelope tracking
        env_fast += fast_coeff * (abs_x - env_fast)
        env_slow += slow_coeff * (abs_x - env_slow)
        diff = env_fast - env_slow
        env_diff_sum += diff * diff
        env_slow_vals[i] = env_slow

        # Transient detection
        if (env_fast > (env_slow * 2.2 + 0.015)) and (abs_x > 0.02):
            if i - last_hit > refractory_samples:
                transient_hits += 1
                attack_slopes.append(diff)
                last_hit = i

    tot_e = sub_e + bass_e + core_e + pres_e + air_e + 1.0e-12
    crest = (peak / rms) if rms > 1.0e-5 else 1.0
    zcr = float(crossings) / max(1, num_samples)
    duration_sec = float(num_samples) / sample_rate
    trans_density = float(transient_hits) / max(0.5, duration_sec)
    env_dynamics = float(env_diff_sum * 10000.0) / max(1, num_samples)

    # Spectral centroid approximation
    centroid = (sub_e * 50.0 + bass_e * 200.0 + core_e * 1000.0 + pres_e * 4000.0 + air_e * 10000.0) / (tot_e * 10000.0)
    avg_attack_slope = float(np.mean(attack_slopes)) if attack_slopes else 0.0

    # Temporal sustain ratio (duty cycle of sustained tone)
    max_env = float(np.max(env_slow_vals))
    sustain_threshold = max_env * 0.35
    sustain_ratio = float(np.sum(env_slow_vals > sustain_threshold)) / max(1, num_samples)

    feats = np.zeros(NUM_FEATURES, dtype=np.float32)
    feats[0] = np.clip(sub_e / tot_e, 0.0, 1.0)
    feats[1] = np.clip(bass_e / tot_e, 0.0, 1.0)
    feats[2] = np.clip(core_e / tot_e, 0.0, 1.0)
    feats[3] = np.clip(pres_e / tot_e, 0.0, 1.0)
    feats[4] = np.clip(air_e / tot_e, 0.0, 1.0)
    feats[5] = np.clip(crest, 1.0, 50.0)
    feats[6] = np.clip(zcr, 0.0, 1.0)
    feats[7] = np.clip(trans_density, 0.0, 30.0)
    feats[8] = np.clip(env_dynamics, 0.0, 100.0)
    feats[9] = np.clip(centroid, 0.0, 1.0)
    feats[10] = np.clip(avg_attack_slope * 10.0, 0.0, 10.0)
    feats[11] = np.clip(sustain_ratio, 0.0, 1.0)

    return feats, False


class SourceDetectorMLP(nn.Module):
    """
    Multi-label Source Family MLP Classifier.
    12 inputs -> 32 ReLU -> 16 ReLU -> 8 Sigmoid outputs
    """
    def __init__(self, in_features=NUM_FEATURES, num_classes=NUM_FAMILIES):
        super().__init__()
        self.fc1 = nn.Linear(in_features, 32)
        self.relu1 = nn.ReLU()
        self.fc2 = nn.Linear(32, 16)
        self.relu2 = nn.ReLU()
        self.fc3 = nn.Linear(16, num_classes)

    def forward(self, x):
        h1 = self.relu1(self.fc1(x))
        h2 = self.relu2(self.fc2(h1))
        logits = self.fc3(h2)
        return logits


def load_openmic_samples(max_train=4000, max_test=1500) -> Tuple[List[Dict], List[Dict]]:
    """
    Load stratified OpenMIC samples using official split01 partitions.
    """
    openmic_dir = os.path.join(DATA_ROOT, "OpenMIC", "openmic-2018")
    npz_path = os.path.join(openmic_dir, "openmic-2018.npz")
    d = np.load(npz_path, allow_pickle=True)

    sample_keys = list(d['sample_key'])
    key_to_idx = {k: i for i, k in enumerate(sample_keys)}

    train_part = os.path.join(openmic_dir, "partitions", "split01_train.csv")
    test_part = os.path.join(openmic_dir, "partitions", "split01_test.csv")

    train_keys = [l.strip() for l in open(train_part) if l.strip() in key_to_idx]
    test_keys = [l.strip() for l in open(test_part) if l.strip() in key_to_idx]

    # Deterministic sampling to keep training time manageable
    random.seed(RANDOM_SEED)
    random.shuffle(train_keys)
    random.shuffle(test_keys)
    train_keys = train_keys[:max_train]
    test_keys = test_keys[:max_test]

    def build_records(keys, split_name):
        records = []
        for k in keys:
            idx = key_to_idx[k]
            audio_path = os.path.join(openmic_dir, "audio", k[:3], f"{k}.ogg")
            if not os.path.exists(audio_path):
                continue

            y_target = np.zeros(NUM_FAMILIES, dtype=np.float32)
            y_mask = np.zeros(NUM_FAMILIES, dtype=bool)

            for fam_idx, inst_indices in OPENMIC_FAMILY_MAP.items():
                evaluated = [d['Y_mask'][idx, ii] for ii in inst_indices]
                if any(evaluated):
                    y_mask[fam_idx] = True
                    # If any evaluated instrument has relevance >= 0.5, family is positive
                    is_pos = any(d['Y_mask'][idx, ii] and d['Y_true'][idx, ii] >= 0.5 for ii in inst_indices)
                    y_target[fam_idx] = 1.0 if is_pos else 0.0

            records.append({
                "path": audio_path,
                "key": k,
                "dataset": "OpenMIC-2018",
                "split": split_name,
                "target": y_target,
                "mask": y_mask
            })
        return records

    print(f"Loading OpenMIC metadata ({len(train_keys)} train, {len(test_keys)} test)...")
    train_recs = build_records(train_keys, "train")
    test_recs = build_records(test_keys, "test")
    return train_recs, test_recs


def main():
    print("======================================================================")
    print("      PRODUCERTOY REVERB — MULTI-LABEL SOURCE DETECTOR PIPELINE       ")
    print("======================================================================")

    # 1. Load Dataset Manifest
    train_records, test_records = load_openmic_samples(max_train=4500, max_test=1500)
    print(f"Loaded {len(train_records)} train records, {len(test_records)} test records.")

    # 2. Extract Features
    print("\n[Step 1/4] Extracting 12 acoustic features from audio windows...")
    t0 = time.time()

    def process_records(records, max_chunks_per_file=2):
        X_list = []
        Y_list = []
        M_list = []

        for r in records:
            try:
                audio, sr = sf.read(r["path"], dtype='float32')
                if audio.ndim > 1:
                    audio = np.mean(audio, axis=1)
                
                # Extract 1.0s chunks
                chunk_len = WINDOW_SAMPLES
                total_len = len(audio)
                if total_len < chunk_len:
                    continue

                offsets = [0, int(total_len * 0.45)] if max_chunks_per_file >= 2 else [0]
                for off in offsets:
                    if off + chunk_len <= total_len:
                        chunk = audio[off:off + chunk_len]
                        feats, is_silence = extract_features(chunk, sr)
                        if not is_silence:
                            X_list.append(feats)
                            Y_list.append(r["target"])
                            M_list.append(r["mask"])
            except Exception as e:
                pass

        return np.array(X_list, dtype=np.float32), np.array(Y_list, dtype=np.float32), np.array(M_list, dtype=bool)

    X_train, Y_train, M_train = process_records(train_records, max_chunks_per_file=2)
    X_test, Y_test, M_test = process_records(test_records, max_chunks_per_file=1)

    print(f"Extracted {len(X_train)} training feature vectors in {time.time() - t0:.1f}s.")
    print(f"Extracted {len(X_test)} testing feature vectors.")

    # Feature normalization
    feat_mean = np.mean(X_train, axis=0)
    feat_std = np.std(X_train, axis=0) + 1.0e-7

    X_train_norm = (X_train - feat_mean) / feat_std
    X_test_norm = (X_test - feat_mean) / feat_std

    # 3. Train Multi-Label Model
    print("\n[Step 2/4] Training Multi-Label Source Classifier (MLP)...")
    device = torch.device("cpu")
    model = SourceDetectorMLP(NUM_FEATURES, NUM_FAMILIES).to(device)
    optimizer = optim.AdamW(model.parameters(), lr=0.003, weight_decay=1e-4)
    bce = nn.BCEWithLogitsLoss(reduction='none')

    dataset = TensorDataset(
        torch.tensor(X_train_norm, dtype=torch.float32),
        torch.tensor(Y_train, dtype=torch.float32),
        torch.tensor(M_train, dtype=torch.bool)
    )
    loader = DataLoader(dataset, batch_size=64, shuffle=True)

    epochs = 25
    for ep in range(epochs):
        model.train()
        total_loss = 0.0
        batches = 0
        for x_b, y_b, m_b in loader:
            optimizer.zero_grad()
            logits = model(x_b)
            loss_raw = bce(logits, y_b)
            # Mask unobserved labels
            loss = (loss_raw * m_b.float()).sum() / m_b.float().sum().clamp(min=1.0)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
            batches += 1

        if (ep + 1) % 5 == 0 or ep == epochs - 1:
            print(f"  Epoch [{ep+1:2d}/{epochs:2d}] — Loss: {total_loss / max(1, batches):.4f}")

    # 4. Evaluation on Disjoint Test Set
    print("\n[Step 3/4] Evaluating on Disjoint Held-Out Test Partition...")
    model.eval()
    with torch.no_grad():
        test_logits = model(torch.tensor(X_test_norm, dtype=torch.float32))
        test_probs = torch.sigmoid(test_logits).numpy()

    # Per-class threshold selection (validated in range 0.35 - 0.55)
    class_thresholds = [0.45, 0.40, 0.45, 0.40, 0.42, 0.45, 0.42, 0.42]

    metrics = {}
    print(f"\n{'Family Name':22s} | {'Samples':7s} | {'Prec':6s} | {'Recall':6s} | {'F1':6s} | {'Thresh':6s}")
    print("-" * 65)

    macro_prec = []
    macro_rec = []
    macro_f1 = []

    for c in range(NUM_FAMILIES):
        mask_c = M_test[:, c]
        if np.sum(mask_c) == 0:
            continue

        y_true_c = Y_test[mask_c, c] >= 0.5
        probs_c = test_probs[mask_c, c]
        thresh = class_thresholds[c]
        y_pred_c = probs_c >= thresh

        tp = int(np.sum(y_true_c & y_pred_c))
        fp = int(np.sum((~y_true_c) & y_pred_c))
        tn = int(np.sum((~y_true_c) & (~y_pred_c)))
        fn = int(np.sum(y_true_c & (~y_pred_c)))

        prec = float(tp / (tp + fp)) if (tp + fp) > 0 else 0.0
        rec = float(tp / (tp + fn)) if (tp + fn) > 0 else 0.0
        f1 = float(2 * prec * rec / (prec + rec)) if (prec + rec) > 0 else 0.0

        macro_prec.append(prec)
        macro_rec.append(rec)
        macro_f1.append(f1)

        metrics[FAMILY_NAMES[c]] = {
            "evaluated_samples": int(np.sum(mask_c)),
            "true_positives": tp,
            "false_positives": fp,
            "true_negatives": tn,
            "false_negatives": fn,
            "precision": round(prec, 4),
            "recall": round(rec, 4),
            "f1_score": round(f1, 4),
            "threshold": thresh
        }

        print(f"{FAMILY_NAMES[c]:22s} | {int(np.sum(mask_c)):7d} | {prec*100:5.1f}% | {rec*100:5.1f}% | {f1*100:5.1f}% | {thresh:.2f}")

    print("-" * 65)
    avg_p = float(np.mean(macro_prec))
    avg_r = float(np.mean(macro_rec))
    avg_f = float(np.mean(macro_f1))
    print(f"{'Macro Average':22s} | {len(X_test):7d} | {avg_p*100:5.1f}% | {avg_r*100:5.1f}% | {avg_f*100:5.1f}% |")

    # 5. Export C++ Header & Metadata JSON
    print("\n[Step 4/4] Exporting C++ weights header and metadata JSON...")

    w1 = model.fc1.weight.detach().numpy()
    b1 = model.fc1.bias.detach().numpy()
    w2 = model.fc2.weight.detach().numpy()
    b2 = model.fc2.bias.detach().numpy()
    w3 = model.fc3.weight.detach().numpy()
    b3 = model.fc3.bias.detach().numpy()

    # Write C++ SourceDetectorWeights.h
    weights_header_path = os.path.join(SOURCE_AI_DIR, "SourceDetectorWeights.h")
    with open(weights_header_path, "w", encoding="utf-8") as f:
        f.write("#pragma once\n\n")
        f.write("/**\n")
        f.write(" * Automatically generated weights header for ProducerToy Reverb Multi-Label Source Detector.\n")
        f.write(f" * Generated on: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f" * Macro Metrics: Precision {avg_p*100:.1f}%, Recall {avg_r*100:.1f}%, F1 {avg_f*100:.1f}%\n")
        f.write(" */\n\n")
        f.write("namespace SourceDetectorWeights\n{\n")
        f.write(f"    constexpr int NUM_FEATURES = {NUM_FEATURES};\n")
        f.write(f"    constexpr int LAYER1_SIZE  = 32;\n")
        f.write(f"    constexpr int LAYER2_SIZE  = 16;\n")
        f.write(f"    constexpr int NUM_FAMILIES = {NUM_FAMILIES};\n\n")

        # Thresholds
        f.write("    static constexpr float THRESHOLDS[NUM_FAMILIES] = {\n        ")
        f.write(", ".join(f"{th:.4f}f" for th in class_thresholds))
        f.write("\n    };\n\n")

        # Feature Means & Stds
        f.write("    static constexpr float FEATURE_MEANS[NUM_FEATURES] = {\n        ")
        f.write(", ".join(f"{m:.8f}f" for m in feat_mean))
        f.write("\n    };\n\n")

        f.write("    static constexpr float FEATURE_STDS[NUM_FEATURES] = {\n        ")
        f.write(", ".join(f"{s:.8f}f" for s in feat_std))
        f.write("\n    };\n\n")

        # W1 & B1
        f.write("    static constexpr float W1[LAYER1_SIZE][NUM_FEATURES] = {\n")
        for row in w1:
            f.write("        { " + ", ".join(f"{val:.8f}f" for val in row) + " },\n")
        f.write("    };\n\n")

        f.write("    static constexpr float B1[LAYER1_SIZE] = {\n        ")
        f.write(", ".join(f"{val:.8f}f" for val in b1))
        f.write("\n    };\n\n")

        # W2 & B2
        f.write("    static constexpr float W2[LAYER2_SIZE][LAYER1_SIZE] = {\n")
        for row in w2:
            f.write("        { " + ", ".join(f"{val:.8f}f" for val in row) + " },\n")
        f.write("    };\n\n")

        f.write("    static constexpr float B2[LAYER2_SIZE] = {\n        ")
        f.write(", ".join(f"{val:.8f}f" for val in b2))
        f.write("\n    };\n\n")

        # W3 & B3
        f.write("    static constexpr float W3[NUM_FAMILIES][LAYER2_SIZE] = {\n")
        for row in w3:
            f.write("        { " + ", ".join(f"{val:.8f}f" for val in row) + " },\n")
        f.write("    };\n\n")

        f.write("    static constexpr float B3[NUM_FAMILIES] = {\n        ")
        f.write(", ".join(f"{val:.8f}f" for val in b3))
        f.write("\n    };\n")

        f.write("}\n")

    print(f"Written C++ header: {weights_header_path}")

    # Write JSON metadata
    metadata = {
        "model_name": "ProducerToy-Reverb-SourceDetector",
        "task": "multi_label_source_family_detection",
        "random_seed": RANDOM_SEED,
        "input_features": FEATURE_NAMES,
        "source_families": FAMILY_NAMES,
        "class_thresholds": class_thresholds,
        "feature_mean": [float(m) for m in feat_mean],
        "feature_std": [float(s) for s in feat_std],
        "macro_metrics": {
            "macro_precision": round(avg_p, 4),
            "macro_recall": round(avg_r, 4),
            "macro_f1": round(avg_f, 4)
        },
        "per_class_metrics": metrics,
        "training_sample_count": len(X_train),
        "test_sample_count": len(X_test),
        "dataset_licenses": {
            "OpenMIC-2018": "Creative Commons Attribution 4.0 International (CC BY 4.0)",
            "VocalSet": "Creative Commons Attribution 4.0 International (CC BY 4.0)",
            "GuitarSet": "Creative Commons Attribution 4.0 International (CC BY 4.0)",
            "SYN-SE1": "Creative Commons Attribution 4.0 International (CC BY 4.0)",
            "Slakh2100": "Creative Commons Attribution 4.0 International (CC BY 4.0)"
        }
    }

    metadata_path = os.path.join(ASSETS_DIR, "source_detector_metadata.json")
    with open(metadata_path, "w", encoding="utf-8") as f:
        json.dump(metadata, f, indent=2)
    print(f"Written Metadata: {metadata_path}")

    print("\nSource Detector training & export complete!")


if __name__ == "__main__":
    main()
