import json
import numpy as np

with open("Assets/model_provenance_candidate.json", "r", encoding="utf-8") as f:
    prov = json.load(f)

classes = [
    "Drums",
    "Vocals",
    "Guitar",
    "Piano / Keys",
    "Bass",
    "Synth / Pad",
    "Other / Uncertain"
]

print("=" * 70)
print("PROVERB AI CANDIDATE MODEL EVALUATION & INTEGRATION AUDIT")
print("=" * 70)

# 1. Dataset & Split statistics
split_counts = prov["split_counts"]
print("\n--- 1. SPLIT TOTALS & SAMPLES ---")
print(f"Train chunks: {split_counts['train']}")
print(f"Val chunks  : {split_counts['val']}")
print(f"Test chunks : {split_counts['test']}")
total_chunks = sum(split_counts.values())
print(f"Total chunks: {total_chunks}")

split_details = prov["split_details"]
print("\n--- 2. PARENT RECORDING SOURCES PER SPLIT ---")
print(f"{'Class':<20} | {'Train Src':<10} | {'Val Src':<10} | {'Test Src':<10} | {'Test Chunks':<12}")
print("-" * 72)
for c in classes:
    tr = split_details["train"].get(c, {})
    va = split_details["val"].get(c, {})
    te = split_details["test"].get(c, {})
    print(f"{c:<20} | {tr.get('sources', 0):<10} | {va.get('sources', 0):<10} | {te.get('sources', 0):<10} | {te.get('chunks', 0):<12}")

# 3. Per-class metrics
metrics = prov["per_class_metrics"]
print("\n--- 3. PER-CLASS EVALUATION METRICS (HELD-OUT TEST SET) ---")
print(f"{'Class':<20} | {'Precision':<10} | {'Recall':<10} | {'F1-Score':<10} | {'Test Count':<10} | {'Status'}")
print("-" * 80)
f1_list = []
weights = []
for c in classes:
    m = metrics[c]
    p = f"{m['precision']*100:.1f}%" if m['precision'] is not None else "N/A"
    r = f"{m['recall']*100:.1f}%" if m['recall'] is not None else "N/A"
    f1 = f"{m['f1_score']*100:.1f}%" if m['f1_score'] is not None else "N/A"
    cnt = m["test_count"]
    status = m["status"]
    print(f"{c:<20} | {p:<10} | {r:<10} | {f1:<10} | {cnt:<10} | {status}")
    if m['f1_score'] is not None:
        f1_list.append(m['f1_score'])
        weights.append(cnt)

macro_f1 = np.mean(f1_list) * 100.0 if f1_list else 0.0
weighted_f1 = (np.sum(np.array(f1_list) * np.array(weights)) / sum(weights)) * 100.0 if weights else 0.0

print("-" * 80)
print(f"Overall Test Accuracy : {prov['held_out_test_accuracy']:.2f}%")
print(f"Macro-Averaged F1     : {macro_f1:.2f}% (across 6 evaluated classes; Vocals excluded as 0 data)")
print(f"Weighted F1           : {weighted_f1:.2f}%")

# 4. Majority class baseline
max_class_count = max(m["test_count"] for m in metrics.values())
majority_baseline = (max_class_count / split_counts["test"]) * 100.0
majority_class = [c for c in classes if metrics[c]["test_count"] == max_class_count][0]
print(f"Majority-Class Baseline: {majority_baseline:.2f}% (Always guessing '{majority_class}')")
print(f"Lift over Baseline     : +{prov['held_out_test_accuracy'] - majority_baseline:.2f}%")
