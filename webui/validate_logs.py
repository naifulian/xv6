#!/usr/bin/env python3
"""Validate xv6 perftest logs before plotting or publishing them."""

import os
import re

LOG_DIR = "log"

COMMON_REQUIRED = [
    "SYS_TOTAL_PAGES",
    "SYS_FREE_PAGES",
    "COW_NO_ACCESS",
    "COW_READONLY",
    "COW_PARTIAL",
    "COW_FULLWRITE",
    "LAZY_SPARSE",
    "LAZY_HALF",
    "LAZY_FULL",
    "BUDDY_FRAGMENT",
]

TESTING_REQUIRED = COMMON_REQUIRED + [
    "MMAP_SPARSE",
    "MMAP_FULL",
    "SCHED_THROUGHPUT_RR",
    "SCHED_THROUGHPUT_FCFS",
    "SCHED_THROUGHPUT_SJF",
    "SCHED_CONVOY_RR",
    "SCHED_CONVOY_FCFS",
    "SCHED_CONVOY_SRTF",
    "SCHED_FAIRNESS_RR",
    "SCHED_FAIRNESS_PRIORITY",
    "SCHED_FAIRNESS_CFS",
    "SCHED_RESPONSE_RR",
    "SCHED_RESPONSE_MLFQ",
]

SCENARIOS = {
    "throughput": ["SCHED_THROUGHPUT_RR", "SCHED_THROUGHPUT_FCFS", "SCHED_THROUGHPUT_SJF"],
    "convoy": ["SCHED_CONVOY_RR", "SCHED_CONVOY_FCFS", "SCHED_CONVOY_SRTF"],
    "fairness": ["SCHED_FAIRNESS_RR", "SCHED_FAIRNESS_PRIORITY", "SCHED_FAIRNESS_CFS"],
    "response": ["SCHED_RESPONSE_RR", "SCHED_RESPONSE_MLFQ"],
}


def parse_log_file(filepath):
    results = {}
    metadata = {}

    if not os.path.exists(filepath):
        return {"metadata": metadata, "results": results, "error": "File not found"}

    content = open(filepath, "r", encoding="utf-8", errors="ignore").read()

    meta_pattern = r"META:ts=(\d+):cpus=(\d+):branch=(\w+):sched=(\w+)"
    meta_matches = re.findall(meta_pattern, content)
    if meta_matches:
        ts, cpus, branch, sched = meta_matches[-1]
        metadata = {
            "timestamp": int(ts),
            "cpus": int(cpus),
            "branch": branch,
            "sched": sched,
        }

    result_pattern = r"RESULT:(\w+):(-?\d+):(\d+):(\w+)(?::(\d+):(\d+))?"
    for name, avg, std, unit, batch, runs in re.findall(result_pattern, content):
        results[name] = {
            "avg": int(avg),
            "std": int(std),
            "unit": unit,
            "batch": int(batch) if batch else 1,
            "runs": int(runs) if runs else 1,
        }

    return {"metadata": metadata, "results": results, "error": None}


def required_tests_for(branch_name):
    return TESTING_REQUIRED if branch_name == "testing" else COMMON_REQUIRED


def validate_metadata(data, branch_name):
    errors = []
    warnings = []
    meta = data["metadata"]

    if not meta:
        return errors, [f"[{branch_name}] No META data found"]

    if meta.get("branch") != branch_name:
        errors.append(f"[{branch_name}] Branch mismatch: expected {branch_name}, got {meta.get('branch')}")
    if meta.get("cpus", 0) < 1:
        errors.append(f"[{branch_name}] Invalid CPU count: {meta.get('cpus')}")

    return errors, warnings


def validate_results(data, branch_name):
    errors = []
    warnings = []
    results = data["results"]
    required = required_tests_for(branch_name)

    for test in required:
        if test not in results:
            errors.append(f"[{branch_name}] Missing required test: {test}")
            continue
        entry = results[test]
        if entry["avg"] < 0 or entry["unit"] == "error":
            errors.append(f"[{branch_name}] Required test {test} failed with invalid result")
        if entry["std"] < 0:
            errors.append(f"[{branch_name}] Invalid std for {test}: {entry['std']}")

    low_resolution = [
        name for name, entry in results.items()
        if entry["unit"] == "ticks"
        and entry["runs"] >= 5
        and entry["std"] == 0
        and entry["avg"] <= 3
    ]
    if len(low_resolution) >= 6:
        warnings.append(
            f"[{branch_name}] Many timing tests are low-resolution (std=0 and avg<=3 ticks, {len(low_resolution)} items); increase workload scale or batch size"
        )

    return errors, warnings


def validate_consistency(baseline_data, testing_data):
    errors = []
    warnings = []
    baseline_names = set(baseline_data["results"].keys())
    testing_names = set(testing_data["results"].keys())

    for test in COMMON_REQUIRED:
        if test not in baseline_names:
            errors.append(f"Missing in baseline: {test}")
        if test not in testing_names:
            errors.append(f"Missing in testing: {test}")

    return errors, warnings


def validate_scheduler_resolution(testing_data):
    warnings = []
    results = testing_data["results"]

    for scenario, tests in SCENARIOS.items():
        values = []
        for test in tests:
            entry = results.get(test)
            if entry and entry["avg"] >= 0 and entry["unit"] != "error":
                values.append(entry["avg"])
        if len(values) >= 3 and len(set(values)) == 1:
            warnings.append(f"[testing] Scheduler scenario {scenario} has identical results; the workload may not distinguish algorithms")

    return warnings


def print_summary(baseline_data, testing_data):
    print()
    print("=" * 60)
    print("LOG FILE SUMMARY")
    print("=" * 60)

    for title, data in (("Baseline", baseline_data), ("Testing", testing_data)):
        print(f"\n[{title}]")
        if data["error"]:
            print(f"  Error: {data['error']}")
            continue
        if data["metadata"]:
            print(f"  Branch: {data['metadata'].get('branch', 'N/A')}")
            print(f"  Scheduler: {data['metadata'].get('sched', 'N/A')}")
            print(f"  CPUs: {data['metadata'].get('cpus', 'N/A')}")
        else:
            print("  No META data")
        print(f"  Results: {len(data['results'])} entries")


def main():
    print("=" * 60)
    print("xv6 Performance Test Log Validator")
    print("=" * 60)

    baseline = parse_log_file(f"{LOG_DIR}/baseline/perftest.log")
    testing = parse_log_file(f"{LOG_DIR}/testing/perftest.log")

    all_errors = []
    all_warnings = []

    for branch_name, data in (("baseline", baseline), ("testing", testing)):
        errors, warnings = validate_metadata(data, branch_name)
        all_errors.extend(errors)
        all_warnings.extend(warnings)
        errors, warnings = validate_results(data, branch_name)
        all_errors.extend(errors)
        all_warnings.extend(warnings)

    errors, warnings = validate_consistency(baseline, testing)
    all_errors.extend(errors)
    all_warnings.extend(warnings)
    all_warnings.extend(validate_scheduler_resolution(testing))

    print_summary(baseline, testing)

    print()
    print("=" * 60)
    print("VALIDATION RESULT")
    print("=" * 60)

    if all_errors:
        print(f"\n[ERRORS] {len(all_errors)}")
        for item in all_errors:
            print(f"  - {item}")

    if all_warnings:
        print(f"\n[WARNINGS] {len(all_warnings)}")
        for item in all_warnings:
            print(f"  - {item}")

    if all_errors:
        print(f"\n[FAILED] {len(all_errors)} errors, {len(all_warnings)} warnings")
        return 1

    if all_warnings:
        print(f"\n[OK] No errors, but {len(all_warnings)} warnings")
    else:
        print("\n[OK] All validations passed!")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
