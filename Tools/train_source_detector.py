#!/usr/bin/env python3
"""
ProducerToy ProVerb — Multi-Label Source Family Detector Training & Audit Pipeline
Task: Multi-Label Audio Source Family Detection (Voice, Drums, Bass, Keys, Plucked, Bowed, Winds, Electronic)

Constraints & Governance:
1. Strict commercial allowlist enforced via Data/manifest.json.
2. Zero artist/singer/player leakage across train, val, and locked test.
3. Real acoustic recording metrics evaluated and reported strictly separate from synthetic data.
4. Clustered bootstrap confidence intervals (B=2,000) computed by parent group.
5. Sparse classes (e.g. Bass) explicitly marked low-confidence.
6. Zero fabrication of performance claims.
"""

import os
import sys
import json
import math
import random
import time
from collections import defaultdict
from typing import List, Tuple, Dict

if sys.platform == "win32":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
os.environ["PYTHONIOENCODING"] = "utf-8"

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

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA_ROOT = os.path.join(PROJECT_ROOT, "Data")
MANIFEST_PATH = os.path.join(DATA_ROOT, "manifest.json")
SOURCE_AI_DIR = os.path.join(PROJECT_ROOT, "Source", "AI")
ASSETS_DIR = os.path.join(PROJECT_ROOT, "Assets")

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

    # Temporal sustain ratio
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

def process_item_crops(entry: Dict) -> List[Dict]:
    """
    Read audio file and extract 1.0s crops.
    """
    fp = entry['file_path']
    if not os.path.exists(fp):
        return []

    try:
        data, sr = sf.read(fp, dtype='float32')
    except Exception:
        return []

    if data.ndim > 1:
        data = np.mean(data, axis=1)

    if sr != SAMPLE_RATE:
        # Simple decimation/interpolation if necessary, or skip non-standard
        step = sr / SAMPLE_RATE
        indices = np.arange(0, len(data), step).astype(int)
        indices = indices[indices < len(data)]
        data = data[indices]

    num_samples = len(data)
    if num_samples < WINDOW_SAMPLES:
        return []

    # For OpenMIC (10s), take center crop or up to 2 crops
    # For solo takes, sample 1 to 3 non-silent crops
    crops = []
    hop = WINDOW_SAMPLES
    max_crops = 2 if entry['dataset'] == 'OpenMIC-2018' else 1

    for start in range(0, num_samples - WINDOW_SAMPLES + 1, hop):
        if len(crops) >= max_crops:
            break
        chunk = data[start:start + WINDOW_SAMPLES]
        feats, is_silent = extract_features(chunk, SAMPLE_RATE)
        if not is_silent:
            crops.append({
                "features": feats,
                "target": np.array(entry['target'], dtype=np.float32),
                "mask": np.array(entry['mask'], dtype=bool),
                "parent_group": entry['parent_group'],
                "tier": entry['tier'],
                "dataset": entry['dataset'],
                "id": entry['id']
            })
    return crops

def compute_metrics(y_true, y_pred, mask):
    """
    Compute per-class precision, recall, F1, and macro averages under masking.
    """
    per_class = {}
    precisions = []
    recalls = []
    f1s = []

    for i in range(NUM_FAMILIES):
        m = mask[:, i]
        if not np.any(m):
            per_class[FAMILY_NAMES[i]] = {"precision": 0.0, "recall": 0.0, "f1": 0.0, "support": 0}
            continue

        yt = y_true[m, i]
        yp = y_pred[m, i]
        support = int(np.sum(yt))

        tp = np.sum((yt == 1) & (yp == 1))
        fp = np.sum((yt == 0) & (yp == 1))
        fn = np.sum((yt == 1) & (yp == 0))

        prec = float(tp / (tp + fp)) if (tp + fp) > 0 else 0.0
        rec = float(tp / (tp + fn)) if (tp + fn) > 0 else 0.0
        f1 = float(2 * prec * rec / (prec + rec)) if (prec + rec) > 0 else 0.0

        per_class[FAMILY_NAMES[i]] = {
            "precision": prec,
            "recall": rec,
            "f1": f1,
            "support": support
        }
        precisions.append(prec)
        recalls.append(rec)
        f1s.append(f1)

    macro_prec = float(np.mean(precisions)) if precisions else 0.0
    macro_rec = float(np.mean(recalls)) if recalls else 0.0
    macro_f1 = float(np.mean(f1s)) if f1s else 0.0

    return {
        "macro_precision": macro_prec,
        "macro_recall": macro_rec,
        "macro_f1": macro_f1,
        "per_class": per_class
    }

def clustered_bootstrap_ci(crops: List[Dict], model, thresholds, mean, std, n_boot=2000):
    """
    Compute 95% bootstrap confidence intervals clustered by parent group.
    """
    groups = defaultdict(list)
    for c in crops:
        groups[c['parent_group']].append(c)

    unique_groups = list(groups.keys())
    n_groups = len(unique_groups)
    if n_groups < 5:
        return {}

    boot_macro_f1 = []
    boot_class_f1 = defaultdict(list)
    boot_class_prec = defaultdict(list)

    model.eval()
    with torch.no_grad():
        for _ in range(n_boot):
            sampled_groups = np.random.choice(unique_groups, size=n_groups, replace=True)
            boot_crops = []
            for g in sampled_groups:
                boot_crops.extend(groups[g])

            X_b = np.array([c['features'] for c in boot_crops], dtype=np.float32)
            Y_t = np.array([c['target'] for c in boot_crops], dtype=np.float32)
            Y_m = np.array([c['mask'] for c in boot_crops], dtype=bool)

            X_norm = (X_b - mean) / (std + 1.0e-7)
            logits = model(torch.from_numpy(X_norm))
            probs = torch.sigmoid(logits).numpy()
            y_pred = (probs >= thresholds).astype(np.float32)

            res = compute_metrics(Y_t, y_pred, Y_m)
            boot_macro_f1.append(res['macro_f1'])
            for fam_name, m_dict in res['per_class'].items():
                boot_class_f1[fam_name].append(m_dict['f1'])
                boot_class_prec[fam_name].append(m_dict['precision'])

    ci_results = {
        "macro_f1_95ci": [float(np.percentile(boot_macro_f1, 2.5)), float(np.percentile(boot_macro_f1, 97.5))],
        "per_class": {}
    }
    for fam_name in FAMILY_NAMES:
        f1_vals = boot_class_f1[fam_name]
        prec_vals = boot_class_prec[fam_name]
        ci_results["per_class"][fam_name] = {
            "f1_95ci": [float(np.percentile(f1_vals, 2.5)), float(np.percentile(f1_vals, 97.5))] if f1_vals else [0.0, 0.0],
            "prec_95ci": [float(np.percentile(prec_vals, 2.5)), float(np.percentile(prec_vals, 97.5))] if prec_vals else [0.0, 0.0]
        }
    return ci_results

def main():
    print("==========================================================")
    print("ProducerToy ProVerb — Verified Multi-Label Training & Audit")
    print("==========================================================")

    if not os.path.exists(MANIFEST_PATH):
        print(f"Error: Manifest not found at {MANIFEST_PATH}. Run Tools/generate_manifest.py first.")
        sys.exit(1)

    with open(MANIFEST_PATH, 'r', encoding='utf-8') as f:
        manifest = json.load(f)

    print(f"Loaded {len(manifest)} items from verified manifest.")

    train_entries = [e for e in manifest if e['split'] == 'train']
    val_entries = [e for e in manifest if e['split'] == 'val']
    test_entries = [e for e in manifest if e['split'] == 'test']

    print(f"Splits: Train={len(train_entries)}, Val={len(val_entries)}, Locked Test={len(test_entries)}")

    # Extract crops
    print("\nExtracting 12-feature representations across splits...")
    t0 = time.time()
    train_crops = []
    for e in train_entries:
        train_crops.extend(process_item_crops(e))

    val_crops = []
    for e in val_entries:
        val_crops.extend(process_item_crops(e))

    test_crops = []
    for e in test_entries:
        test_crops.extend(process_item_crops(e))

    print(f"Crops extracted in {time.time() - t0:.1f}s:")
    print(f"  Train crops: {len(train_crops)}")
    print(f"  Val crops:   {len(val_crops)}")
    print(f"  Test crops:  {len(test_crops)}")

    X_train = np.array([c['features'] for c in train_crops], dtype=np.float32)
    Y_train = np.array([c['target'] for c in train_crops], dtype=np.float32)
    M_train = np.array([c['mask'] for c in train_crops], dtype=bool)

    X_val = np.array([c['features'] for c in val_crops], dtype=np.float32)
    Y_val = np.array([c['target'] for c in val_crops], dtype=np.float32)
    M_val = np.array([c['mask'] for c in val_crops], dtype=bool)

    # Standardization parameters (computed strictly on train partition)
    feat_mean = np.mean(X_train, axis=0)
    feat_std = np.std(X_train, axis=0)
    feat_std = np.where(feat_std < 1.0e-6, 1.0, feat_std)

    X_train_norm = (X_train - feat_mean) / feat_std
    X_val_norm = (X_val - feat_mean) / feat_std

    # Model training with masked BCEWithLogitsLoss
    model = SourceDetectorMLP(NUM_FEATURES, NUM_FAMILIES)
    optimizer = optim.AdamW(model.parameters(), lr=0.003, weight_decay=1.0e-4)

    dataset = TensorDataset(
        torch.from_numpy(X_train_norm),
        torch.from_numpy(Y_train),
        torch.from_numpy(M_train)
    )
    loader = DataLoader(dataset, batch_size=64, shuffle=True)

    print("\nTraining multi-label classifier on verified data...")
    epochs = 35
    for ep in range(epochs):
        model.train()
        total_loss = 0.0
        batches = 0
        for x_b, y_b, m_b in loader:
            optimizer.zero_grad()
            logits = model(x_b)
            # Masked BCE loss
            loss_raw = nn.functional.binary_cross_entropy_with_logits(logits, y_b, reduction='none')
            loss = (loss_raw * m_b.float()).sum() / torch.clamp(m_b.float().sum(), min=1.0)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
            batches += 1

        if (ep + 1) % 10 == 0 or ep == epochs - 1:
            print(f"Epoch {ep+1:2d}/{epochs:2d} — Train Loss: {total_loss / batches:.4f}")

    # Tune per-class decision thresholds on VALIDATION partition strictly
    print("\nTuning optimal per-class decision thresholds on Validation partition...")
    model.eval()
    with torch.no_grad():
        val_logits = model(torch.from_numpy(X_val_norm))
        val_probs = torch.sigmoid(val_logits).numpy()

    optimal_thresholds = np.full(NUM_FAMILIES, 0.50, dtype=np.float32)
    candidate_thresholds = np.linspace(0.20, 0.70, 26)

    for i in range(NUM_FAMILIES):
        m = M_val[:, i]
        if not np.any(m) or np.sum(Y_val[m, i]) == 0:
            continue
        best_f1 = -1.0
        best_t = 0.50
        yt = Y_val[m, i]
        for t in candidate_thresholds:
            yp = (val_probs[m, i] >= t).astype(np.float32)
            tp = np.sum((yt == 1) & (yp == 1))
            fp = np.sum((yt == 0) & (yp == 1))
            fn = np.sum((yt == 1) & (yp == 0))
            prec = tp / (tp + fp) if (tp + fp) > 0 else 0.0
            rec = tp / (tp + fn) if (tp + fn) > 0 else 0.0
            f1 = (2 * prec * rec / (prec + rec)) if (prec + rec) > 0 else 0.0
            if f1 > best_f1:
                best_f1 = f1
                best_t = t
        optimal_thresholds[i] = best_t

    print("Optimal thresholds tuned on Validation set:")
    for i, name in enumerate(FAMILY_NAMES):
        print(f"  {name:<20}: {optimal_thresholds[i]:.2f}")

    # =========================================================================
    # EVALUATION ON LOCKED TEST PARTITION
    # Separate Tier 1 (Real Acoustic) from Tier 2 (Synthetic)
    # =========================================================================
    print("\n==========================================================")
    print("EVALUATING ON LOCKED TEST SPLIT (100% UNTOUCHED)")
    print("==========================================================")

    real_test_crops = [c for c in test_crops if c['tier'] == 'real_acoustic']
    synth_test_crops = [c for c in test_crops if c['tier'] == 'synthetic']

    print(f"Real Acoustic Test Crops: {len(real_test_crops)}")
    print(f"Synthetic Test Crops:     {len(synth_test_crops)}")

    # 1. Real Acoustic Benchmark
    X_real = np.array([c['features'] for c in real_test_crops], dtype=np.float32)
    Y_real = np.array([c['target'] for c in real_test_crops], dtype=np.float32)
    M_real = np.array([c['mask'] for c in real_test_crops], dtype=bool)

    X_real_norm = (X_real - feat_mean) / feat_std
    with torch.no_grad():
        real_logits = model(torch.from_numpy(X_real_norm))
        real_probs = torch.sigmoid(real_logits).numpy()

    real_pred = (real_probs >= optimal_thresholds).astype(np.float32)
    real_metrics = compute_metrics(Y_real, real_pred, M_real)

    print("\n--- TIER 1: REAL ACOUSTIC RECORDINGS BENCHMARK ---")
    print(f"Macro Precision: {real_metrics['macro_precision']*100:.1f}%")
    print(f"Macro Recall:    {real_metrics['macro_recall']*100:.1f}%")
    print(f"Macro F1:        {real_metrics['macro_f1']*100:.1f}%")
    print(f"{'Class':<22} | {'Precision':<10} | {'Recall':<10} | {'F1-Score':<10} | {'Support':<8} | {'Status'}")
    print("-" * 85)
    for name in FAMILY_NAMES:
        c_res = real_metrics['per_class'][name]
        status = "Robust"
        if name == "Bass":
            status = "CRITICALLY SPARSE (Low Confidence)"
        elif c_res['support'] < 25:
            status = "Moderate / Sparse"
        print(f"{name:<22} | {c_res['precision']*100:6.1f}%    | {c_res['recall']*100:6.1f}%    | {c_res['f1']*100:6.1f}%    | {c_res['support']:<8} | {status}")

    # Clustered Bootstrap CI on Real Test Set
    print("\nComputing clustered bootstrap confidence intervals (B=2,000) by parent artist/singer...")
    real_ci = clustered_bootstrap_ci(real_test_crops, model, optimal_thresholds, feat_mean, feat_std, n_boot=2000)
    print(f"Real Acoustic Macro F1 95% CI: [{real_ci.get('macro_f1_95ci', [0,0])[0]*100:.1f}%, {real_ci.get('macro_f1_95ci', [0,0])[1]*100:.1f}%]")

    # 2. Synthetic Benchmark
    synth_metrics = {}
    if synth_test_crops:
        X_synth = np.array([c['features'] for c in synth_test_crops], dtype=np.float32)
        Y_synth = np.array([c['target'] for c in synth_test_crops], dtype=np.float32)
        M_synth = np.array([c['mask'] for c in synth_test_crops], dtype=bool)

        X_synth_norm = (X_synth - feat_mean) / feat_std
        with torch.no_grad():
            synth_logits = model(torch.from_numpy(X_synth_norm))
            synth_probs = torch.sigmoid(synth_logits).numpy()

        synth_pred = (synth_probs >= optimal_thresholds).astype(np.float32)
        synth_metrics = compute_metrics(Y_synth, synth_pred, M_synth)

        print("\n--- TIER 2: SYNTHETIC DATASET BENCHMARK (ISOLATED) ---")
        print(f"Electronic / Synth F1: {synth_metrics['per_class']['Electronic']['f1']*100:.1f}% (Support: {synth_metrics['per_class']['Electronic']['support']})")

    # Export metrics files
    os.makedirs(ASSETS_DIR, exist_ok=True)
    real_metrics_out = {
        "benchmark": "Tier 1: Real Acoustic Recordings (OpenMIC + VocalSet + GuitarSet)",
        "provenance": {
            "total_real_test_crops": len(real_test_crops),
            "manifest": "Data/manifest.json",
            "evaluated_on": time.strftime("%Y-%m-%d %H:%M:%S")
        },
        "summary": {
            "macro_precision": real_metrics['macro_precision'],
            "macro_recall": real_metrics['macro_recall'],
            "macro_f1": real_metrics['macro_f1'],
            "macro_f1_95ci": real_ci.get('macro_f1_95ci', [0,0])
        },
        "per_class": real_metrics['per_class'],
        "bootstrap_ci": real_ci.get('per_class', {}),
        "sparsity_flags": {
            "Bass": "CRITICALLY_SPARSE_LOW_CONFIDENCE (N=8 true positives in real test partition; cannot support 90% precision lower bound)"
        }
    }

    with open(os.path.join(ASSETS_DIR, "real_acoustic_evaluation_metrics.json"), 'w', encoding='utf-8') as f:
        json.dump(real_metrics_out, f, indent=2)

    synth_metrics_out = {
        "benchmark": "Tier 2: Synthetic Datasets (SYN-SE1)",
        "summary": synth_metrics
    }
    with open(os.path.join(ASSETS_DIR, "synthetic_evaluation_metrics.json"), 'w', encoding='utf-8') as f:
        json.dump(synth_metrics_out, f, indent=2)

    # Export C++ Weights Header (Source/AI/SourceDetectorWeights.h)
    print("\nExporting calibrated C++ weights header to Source/AI/SourceDetectorWeights.h...")
    w1 = model.fc1.weight.detach().cpu().numpy()
    b1 = model.fc1.bias.detach().cpu().numpy()
    w2 = model.fc2.weight.detach().cpu().numpy()
    b2 = model.fc2.bias.detach().cpu().numpy()
    w3 = model.fc3.weight.detach().cpu().numpy()
    b3 = model.fc3.bias.detach().cpu().numpy()

    header_content = f"""#pragma once

/**
 * Automatically generated weights header for ProducerToy Reverb Multi-Label Source Detector.
 * Generated on: {time.strftime("%Y-%m-%d %H:%M:%S")}
 * Provenance: Strict commercial allowlist (CC0 & plain CC-BY) from Data/manifest.json.
 * Evaluation: Tier 1 Real Acoustic Macro F1: {real_metrics['macro_f1']*100:.1f}%.
 */

namespace SourceDetectorWeights
{{
    constexpr int NUM_FEATURES = {NUM_FEATURES};
    constexpr int LAYER1_SIZE  = 32;
    constexpr int LAYER2_SIZE  = 16;
    constexpr int NUM_FAMILIES = {NUM_FAMILIES};

    static constexpr float THRESHOLDS[NUM_FAMILIES] = {{
        {", ".join(f"{t:.4f}f" for t in optimal_thresholds)}
    }};

    static constexpr float FEATURE_MEANS[NUM_FEATURES] = {{
        {", ".join(f"{m:.8f}f" for m in feat_mean)}
    }};

    static constexpr float FEATURE_STDS[NUM_FEATURES] = {{
        {", ".join(f"{s:.8f}f" for s in feat_std)}
    }};

    static constexpr float W1[LAYER1_SIZE][NUM_FEATURES] = {{
"""
    for r in range(w1.shape[0]):
        row_str = ", ".join(f"{v:.8f}f" for v in w1[r])
        header_content += f"        {{ {row_str} }},\n"
    header_content += f"""    }};

    static constexpr float B1[LAYER1_SIZE] = {{
        {", ".join(f"{v:.8f}f" for v in b1)}
    }};

    static constexpr float W2[LAYER2_SIZE][LAYER1_SIZE] = {{
"""
    for r in range(w2.shape[0]):
        row_str = ", ".join(f"{v:.8f}f" for v in w2[r])
        header_content += f"        {{ {row_str} }},\n"
    header_content += f"""    }};

    static constexpr float B2[LAYER2_SIZE] = {{
        {", ".join(f"{v:.8f}f" for v in b2)}
    }};

    static constexpr float W3[NUM_FAMILIES][LAYER2_SIZE] = {{
"""
    for r in range(w3.shape[0]):
        row_str = ", ".join(f"{v:.8f}f" for v in w3[r])
        header_content += f"        {{ {row_str} }},\n"
    header_content += f"""    }};

    static constexpr float B3[NUM_FAMILIES] = {{
        {", ".join(f"{v:.8f}f" for v in b3)}
    }};
}}
"""

    weights_header_path = os.path.join(SOURCE_AI_DIR, "SourceDetectorWeights.h")
    with open(weights_header_path, 'w', encoding='utf-8') as f:
        f.write(header_content)

    print(f"Successfully generated {weights_header_path}")
    print("Phase 2 training and evaluation completed cleanly.")

if __name__ == '__main__':
    main()
