import json
import os
import numpy as np
import onnxruntime as ort
from collections import Counter

MANIFEST_PATH = "Assets/training_data_manifest.json"
MODEL_CANDIDATE_PATH = "Assets/ProVerb_AI_candidate.onnx"
NORM_CANDIDATE_PATH = "Assets/feature_norm_params_candidate.json"
MODEL_PROD_PATH = "Assets/ProVerb_AI.onnx"
NORM_PROD_PATH = "Assets/feature_norm_params.json"

CLASSES = [
    "Drums",
    "Vocals",
    "Guitar",
    "Piano / Keys",
    "Bass",
    "Synth / Pad",
    "Other / Uncertain"
]

def load_manifest():
    with open(MANIFEST_PATH, "r", encoding="utf-8") as f:
        return json.load(f)

def audit_onnx_model(model_path):
    sess = ort.InferenceSession(model_path)
    inputs = sess.get_inputs()
    outputs = sess.get_outputs()
    return {
        "inputs": [{"name": i.name, "shape": i.shape, "type": i.type} for i in inputs],
        "outputs": [{"name": o.name, "shape": o.shape, "type": o.type} for o in outputs]
    }

def main():
    print("=" * 60)
    print("1. ONNX MODEL ARTIFACT VERIFICATION")
    print("=" * 60)
    cand_info = audit_onnx_model(MODEL_CANDIDATE_PATH)
    prod_info = audit_onnx_model(MODEL_PROD_PATH)
    print("Candidate ONNX Inputs :", cand_info["inputs"])
    print("Candidate ONNX Outputs:", cand_info["outputs"])
    print("Production ONNX Inputs :", prod_info["inputs"])
    print("Production ONNX Outputs:", prod_info["outputs"])
    
    # 2. Dataset Split & Evaluation
    print("\n" + "=" * 60)
    print("2. DATASET SPLIT & EVALUATION RELIABILITY AUDIT")
    print("=" * 60)
    manifest = load_manifest()
    records = manifest.get("records", [])
    print(f"Total manifest records: {len(records)}")
    
    # Inspect multitrack groups and split assignment
    # Follow the grouping strategy from train_proverb_model.py
    import hashlib
    def get_group_id(rec):
        path = rec.get("file_path", "")
        # Normalize slashes
        parts = path.replace("\\", "/").split("/")
        # Look for TrackXXXXX
        for p in parts:
            if p.startswith("Track") and len(p) >= 6:
                return p
        # Fallback to parent dir or filename
        return parts[-2] if len(parts) > 1 else parts[-1]
    
    groups = {}
    for r in records:
        gid = get_group_id(r)
        groups.setdefault(gid, []).append(r)
        
    print(f"Total independent parent recording groups: {len(groups)}")
    for gid, items in sorted(groups.items()):
        classes_in_group = Counter(it["target_class"] for it in items)
        print(f"  {gid}: {len(items)} stems | Classes: {dict(classes_in_group)}")

    # Reproduce deterministic split by group hash (as in train_proverb_model.py)
    train_groups = []
    val_groups = []
    test_groups = []
    
    for gid in sorted(groups.keys()):
        h = int(hashlib.md5(gid.encode("utf-8")).hexdigest(), 16) % 100
        if h < 70:
            train_groups.append(gid)
        elif h < 85:
            val_groups.append(gid)
        else:
            test_groups.append(gid)
            
    print(f"\nGroup Split Counts -> Train: {len(train_groups)}, Val: {len(val_groups)}, Test: {len(test_groups)}")
    print(f"  Train Groups: {train_groups}")
    print(f"  Val Groups  : {val_groups}")
    print(f"  Test Groups : {test_groups}")
    
    # Verify zero leakage across groups
    assert set(train_groups).isdisjoint(set(val_groups)), "Leakage between train and val!"
    assert set(train_groups).isdisjoint(set(test_groups)), "Leakage between train and test!"
    assert set(val_groups).isdisjoint(set(test_groups)), "Leakage between val and test!"
    print("Zero-leakage verification: PASSED (No parent track appears in more than one split).")

    # Load candidate norm params
    with open(NORM_CANDIDATE_PATH, "r", encoding="utf-8") as f:
        norm_cand = json.load(f)
    mean_cand = np.array(norm_cand["mean"], dtype=np.float32)
    std_cand = np.array(norm_cand["std"], dtype=np.float32)
    std_cand = np.where(std_cand < 1e-6, 1.0, std_cand)

    # Collect test stems and chunk features
    test_records = [r for r in records if get_group_id(r) in test_groups]
    print(f"\nTotal test records (stems): {len(test_records)}")
    test_classes = Counter(r["target_class"] for r in test_records)
    print("Test records per class:", dict(test_classes))
    
    # Run candidate session
    sess_cand = ort.InferenceSession(MODEL_CANDIDATE_PATH)
    
    # Extract features from test records directly if features are extracted, or extract using features in manifest
    # Let's inspect what fields records have
    first_r = records[0]
    print("Record sample keys:", list(first_r.keys()))
    
    # Check if chunks are pre-extracted or if we need feature extractor
    has_features = "features" in first_r or "feature_chunks" in first_r
    print("Records have precomputed features in manifest:", has_features)

if __name__ == "__main__":
    main()
