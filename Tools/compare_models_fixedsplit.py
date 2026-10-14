#!/usr/bin/env python3
"""Compare staged ProVerb ONNX models on one immutable, source-held-out set."""

import argparse
import hashlib
import json
import os
from pathlib import Path

import numpy as np
import onnxruntime as ort

from train_proverb_model import CLASS_NAMES, group_split_dataset, load_dataset_from_manifest


def score_predictions(y_true: np.ndarray, y_pred: np.ndarray) -> dict:
    per_class = {}
    f1_values = []
    for class_index, class_name in enumerate(CLASS_NAMES):
        true_positive = int(np.sum((y_true == class_index) & (y_pred == class_index)))
        false_positive = int(np.sum((y_true != class_index) & (y_pred == class_index)))
        false_negative = int(np.sum((y_true == class_index) & (y_pred != class_index)))
        precision = true_positive / (true_positive + false_positive) if true_positive + false_positive else 0.0
        recall = true_positive / (true_positive + false_negative) if true_positive + false_negative else 0.0
        f1 = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
        support = int(np.sum(y_true == class_index))
        per_class[class_name] = {
            "precision": precision,
            "recall": recall,
            "f1": f1,
            "support": support,
        }
        if support:
            f1_values.append(f1)

    return {
        "accuracy": float(np.mean(y_true == y_pred)),
        "macro_f1": float(np.mean(f1_values)) if f1_values else 0.0,
        "per_class": per_class,
    }


def evaluate_model(model_path: str, X_test: np.ndarray, y_test: np.ndarray) -> dict:
    session = ort.InferenceSession(model_path, providers=["CPUExecutionProvider"])
    input_name = session.get_inputs()[0].name
    output = session.run(None, {input_name: np.ascontiguousarray(X_test, dtype=np.float32)})[0]
    predictions = np.argmax(output, axis=1).astype(np.int64)
    return score_predictions(y_test, predictions)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, help="Baseline manifest defining the common test set")
    parser.add_argument("--baseline", required=True, help="Baseline ONNX model")
    parser.add_argument("--candidate", required=True, help="Expanded-data ONNX model")
    parser.add_argument(
        "--additional-model", action="append", nargs=2, metavar=("LABEL", "PATH"), default=[],
        help="Extra staged model to score on the identical held-out set; repeatable",
    )
    parser.add_argument("--output", required=True, help="JSON report path")
    args = parser.parse_args()

    X, y, source_ids, _ = load_dataset_from_manifest(args.manifest)
    split = group_split_dataset(X, y, source_ids)
    test_indices = split["test_idx"]
    X_test, y_test = X[test_indices], y[test_indices]
    heldout_source_keys = sorted({f"{int(y[index])}:{source_ids[index]}" for index in test_indices})
    source_fingerprint = hashlib.sha256("\n".join(heldout_source_keys).encode("utf-8")).hexdigest()

    baseline_scores = evaluate_model(args.baseline, X_test, y_test)
    candidate_scores = evaluate_model(args.candidate, X_test, y_test)
    additional_scores = {
        label: evaluate_model(model_path, X_test, y_test)
        for label, model_path in args.additional_model
    }
    report = {
        "comparison": "same_baseline_manifest_source_heldout_test_set",
        "manifest": os.path.basename(args.manifest),
        "test_samples": int(len(test_indices)),
        "test_source_groups": len(heldout_source_keys),
        "test_source_fingerprint_sha256": source_fingerprint,
        "models": {
            "baseline": {"model": os.path.basename(args.baseline), **baseline_scores},
            "expanded_candidate": {"model": os.path.basename(args.candidate), **candidate_scores},
            **{
                label: {"model": os.path.basename(model_path), **additional_scores[label]}
                for label, model_path in args.additional_model
            },
        },
        "delta_candidate_minus_baseline": {
            "accuracy": candidate_scores["accuracy"] - baseline_scores["accuracy"],
            "macro_f1": candidate_scores["macro_f1"] - baseline_scores["macro_f1"],
            "per_class_f1": {
                name: candidate_scores["per_class"][name]["f1"] - baseline_scores["per_class"][name]["f1"]
                for name in CLASS_NAMES
            },
        },
        "additional_model_deltas_vs_baseline": {
            label: {
                "accuracy": scores["accuracy"] - baseline_scores["accuracy"],
                "macro_f1": scores["macro_f1"] - baseline_scores["macro_f1"],
                "per_class_f1": {
                    name: scores["per_class"][name]["f1"] - baseline_scores["per_class"][name]["f1"]
                    for name in CLASS_NAMES
                },
            }
            for label, scores in additional_scores.items()
        },
    }

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    print(f"Common held-out set: {len(test_indices)} chunks / {len(heldout_source_keys)} source groups")
    print(f"Source fingerprint: {source_fingerprint}")
    print("Model              Accuracy   Macro-F1")
    for key, scores in report["models"].items():
        print(f"{key:<24} {scores['accuracy']:.4f}     {scores['macro_f1']:.4f}")
    print("\nPer-class F1 (baseline -> expanded; delta):")
    for class_name in CLASS_NAMES:
        before = baseline_scores["per_class"][class_name]["f1"]
        after = candidate_scores["per_class"][class_name]["f1"]
        print(f"  {class_name:<20} {before:.4f} -> {after:.4f} ({after - before:+.4f})")
    print(f"\nReport saved: {output_path}")


if __name__ == "__main__":
    main()
