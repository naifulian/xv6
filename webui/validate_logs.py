#!/usr/bin/env python3
"""
validate_logs.py - Validate performance test log files

Checks:
1. All required test names are present
2. Naming is consistent between baseline and testing
3. Values are within reasonable ranges
4. META data is present
"""

import re
import os
import sys
from pathlib import Path

LOG_DIR = "log"

# Required test names that must be present
REQUIRED_TESTS = {
    'memory': [
        'COW_NO_ACCESS',
        'COW_READONLY',
        'COW_PARTIAL',
        'COW_FULLWRITE',
        'LAZY_SPARSE',
        'LAZY_HALF',
        'LAZY_FULL',
        'BUDDY_FRAGMENT',
    ],
    'system': [
        'SYS_TOTAL_PAGES',
        'SYS_FREE_PAGES',
    ],
    'scheduler': [
        'SCHED_THROUGHPUT_RR',
        'SCHED_THROUGHPUT_FCFS',
        'SCHED_THROUGHPUT_SJF',
    ],
}


def parse_log_file(filepath):
    """Parse log file and extract all RESULT lines"""
    results = {}
    metadata = {}
    
    if not os.path.exists(filepath):
        return {'metadata': metadata, 'results': results, 'error': 'File not found'}
    
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    
    # Parse META lines
    meta_pattern = r'META:ts=(\d+):cpus=(\d+):branch=(\w+):sched=(\w+)'
    meta_matches = re.findall(meta_pattern, content)
    if meta_matches:
        ts, cpus, branch, sched = meta_matches[-1]
        metadata = {
            'timestamp': int(ts),
            'cpus': int(cpus),
            'branch': branch,
            'sched': sched
        }
    
    # Parse RESULT lines
    result_pattern = r'RESULT:(\w+):(-?\d+):(\d+):(\w+)(?::(\d+):(\d+))?'
    result_matches = re.findall(result_pattern, content)
    
    for match in result_matches:
        name, avg, std, unit = match[0], match[1], match[2], match[3]
        batch = int(match[4]) if match[4] else 1
        runs = int(match[5]) if match[5] else 1
        results[name] = {
            'avg': int(avg),
            'std': int(std),
            'unit': unit,
            'batch': batch,
            'runs': runs
        }
    
    return {'metadata': metadata, 'results': results, 'error': None}


def validate_metadata(data, branch_name):
    """Validate META data"""
    errors = []
    warnings = []
    
    if not data['metadata']:
        warnings.append(f"[{branch_name}] No META data found")
        return errors, warnings
    
    meta = data['metadata']
    
    if meta.get('branch') != branch_name:
        errors.append(f"[{branch_name}] Branch mismatch in META: expected '{branch_name}', got '{meta.get('branch')}'")
    
    if meta.get('cpus', 0) < 1:
        warnings.append(f"[{branch_name}] Invalid CPU count: {meta.get('cpus')}")
    
    return errors, warnings


def validate_results(data, branch_name, required_tests):
    """Validate RESULT data"""
    errors = []
    warnings = []
    
    results = data['results']
    
    # Check for missing tests
    for category, tests in required_tests.items():
        for test in tests:
            if test not in results:
                if category == 'scheduler' and branch_name == 'baseline':
                    continue  # Scheduler tests are only for testing branch
                warnings.append(f"[{branch_name}] Missing test: {test}")
    
    # Check for error values
    for name, val in results.items():
        if val['avg'] == -1:
            warnings.append(f"[{branch_name}] Test {name} returned error (-1)")
        if val['avg'] < 0 and val['avg'] != -1:
            errors.append(f"[{branch_name}] Invalid negative value for {name}: {val['avg']}")
        if val['std'] < 0:
            errors.append(f"[{branch_name}] Invalid negative std for {name}: {val['std']}")
    
    return errors, warnings


def validate_consistency(baseline_data, testing_data):
    """Validate naming consistency between branches"""
    errors = []
    warnings = []
    
    baseline_names = set(baseline_data['results'].keys())
    testing_names = set(testing_data['results'].keys())
    
    # Tests that should be in both
    common_tests = {
        'COW_NO_ACCESS', 'COW_READONLY', 'COW_PARTIAL', 'COW_FULLWRITE',
        'LAZY_SPARSE', 'LAZY_HALF', 'LAZY_FULL',
        'BUDDY_FRAGMENT',
        'SYS_TOTAL_PAGES', 'SYS_FREE_PAGES',
    }
    
    for test in common_tests:
        in_baseline = test in baseline_names
        in_testing = test in testing_names
        
        if not in_baseline and not in_testing:
            errors.append(f"Missing in both branches: {test}")
        elif not in_baseline:
            warnings.append(f"Missing in baseline: {test}")
        elif not in_testing:
            warnings.append(f"Missing in testing: {test}")
    
    return errors, warnings


def print_summary(baseline_data, testing_data):
    """Print summary of parsed data"""
    print()
    print("=" * 60)
    print("LOG FILE SUMMARY")
    print("=" * 60)
    
    for name, data in [('Baseline', baseline_data), ('Testing', testing_data)]:
        print(f"\n[{name}]")
        
        if data['error']:
            print(f"  Error: {data['error']}")
            continue
        
        if data['metadata']:
            print(f"  Branch: {data['metadata'].get('branch', 'N/A')}")
            print(f"  Scheduler: {data['metadata'].get('sched', 'N/A')}")
            print(f"  CPUs: {data['metadata'].get('cpus', 'N/A')}")
        else:
            print("  No META data")
        
        print(f"  Results: {len(data['results'])} entries")
        
        if data['results']:
            print("  Sample entries:")
            for i, (name, val) in enumerate(list(data['results'].items())[:5]):
                print(f"    {name}: {val['avg']} +/- {val['std']} {val['unit']}")


def main():
    print("=" * 60)
    print("xv6 Performance Test Log Validator")
    print("=" * 60)
    
    baseline_file = f"{LOG_DIR}/baseline/perftest.log"
    testing_file = f"{LOG_DIR}/testing/perftest.log"
    
    print()
    print("[Step 1] Parsing log files...")
    
    baseline_data = parse_log_file(baseline_file)
    testing_data = parse_log_file(testing_file)
    
    all_errors = []
    all_warnings = []
    
    print()
    print("[Step 2] Validating metadata...")
    errors, warnings = validate_metadata(baseline_data, 'baseline')
    all_errors.extend(errors)
    all_warnings.extend(warnings)
    
    errors, warnings = validate_metadata(testing_data, 'testing')
    all_errors.extend(errors)
    all_warnings.extend(warnings)
    
    print()
    print("[Step 3] Validating results...")
    errors, warnings = validate_results(baseline_data, 'baseline', REQUIRED_TESTS)
    all_errors.extend(errors)
    all_warnings.extend(warnings)
    
    errors, warnings = validate_results(testing_data, 'testing', REQUIRED_TESTS)
    all_errors.extend(errors)
    all_warnings.extend(warnings)
    
    print()
    print("[Step 4] Validating naming consistency...")
    errors, warnings = validate_consistency(baseline_data, testing_data)
    all_errors.extend(errors)
    all_warnings.extend(warnings)
    
    print_summary(baseline_data, testing_data)
    
    print()
    print("=" * 60)
    print("VALIDATION RESULT")
    print("=" * 60)
    
    if all_errors:
        print(f"\n[ERRORS] {len(all_errors)}")
        for err in all_errors:
            print(f"  - {err}")
    
    if all_warnings:
        print(f"\n[WARNINGS] {len(all_warnings)}")
        for warn in all_warnings:
            print(f"  - {warn}")
    
    if not all_errors and not all_warnings:
        print("\n[OK] All validations passed!")
    elif not all_errors:
        print(f"\n[OK] No errors, but {len(all_warnings)} warnings")
    else:
        print(f"\n[FAILED] {len(all_errors)} errors, {len(all_warnings)} warnings")
        return 1
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
