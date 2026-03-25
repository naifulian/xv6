#!/usr/bin/env python3
"""Shared loaders and aggregators for module-three experiment artifacts."""

from __future__ import annotations

import csv
import hashlib
import math
import os
import re
from pathlib import Path

LOG_DIR = Path(os.environ.get('XV6_LOG_DIR', 'log'))

COMMON_REQUIRED = [
    'SYS_TOTAL_PAGES',
    'SYS_FREE_PAGES',
    'COW_NO_ACCESS',
    'COW_READONLY',
    'COW_PARTIAL',
    'COW_FULLWRITE',
    'LAZY_SPARSE',
    'LAZY_HALF',
    'LAZY_FULL',
]

TESTING_REQUIRED = COMMON_REQUIRED + [
    'SCHED_THROUGHPUT_RR',
    'SCHED_THROUGHPUT_FCFS',
    'SCHED_THROUGHPUT_SJF',
    'SCHED_CONVOY_RR',
    'SCHED_CONVOY_FCFS',
    'SCHED_CONVOY_SRTF',
    'SCHED_FAIRNESS_RR',
    'SCHED_FAIRNESS_PRIORITY',
    'SCHED_FAIRNESS_CFS',
    'SCHED_RESPONSE_RR',
    'SCHED_RESPONSE_MLFQ',
]

SCENARIOS = {
    'throughput': ['SCHED_THROUGHPUT_RR', 'SCHED_THROUGHPUT_FCFS', 'SCHED_THROUGHPUT_SJF'],
    'convoy': ['SCHED_CONVOY_RR', 'SCHED_CONVOY_FCFS', 'SCHED_CONVOY_SRTF'],
    'fairness': ['SCHED_FAIRNESS_RR', 'SCHED_FAIRNESS_PRIORITY', 'SCHED_FAIRNESS_CFS'],
    'response': ['SCHED_RESPONSE_RR', 'SCHED_RESPONSE_MLFQ'],
}

TEST_META = {
    'SYS_TOTAL_PAGES': {'label': 'Total Physical Pages', 'category': 'system', 'objective': 'higher'},
    'SYS_FREE_PAGES': {'label': 'Free Physical Pages', 'category': 'system', 'objective': 'higher'},
    'COW_NO_ACCESS': {'label': 'COW No Access', 'category': 'cow', 'objective': 'lower'},
    'COW_READONLY': {'label': 'COW Read Only', 'category': 'cow', 'objective': 'lower'},
    'COW_PARTIAL': {'label': 'COW Write 30%', 'category': 'cow', 'objective': 'lower'},
    'COW_FULLWRITE': {'label': 'COW Write 100%', 'category': 'cow', 'objective': 'lower'},
    'LAZY_SPARSE': {'label': 'Lazy Access 1%', 'category': 'lazy', 'objective': 'lower'},
    'LAZY_HALF': {'label': 'Lazy Access 50%', 'category': 'lazy', 'objective': 'lower'},
    'LAZY_FULL': {'label': 'Lazy Access 100%', 'category': 'lazy', 'objective': 'lower'},
    'BUDDY_FRAGMENT': {'label': 'Buddy Max Allocation', 'category': 'buddy', 'objective': 'higher'},
    'MMAP_SPARSE': {'label': 'mmap Access 1%', 'category': 'mmap', 'objective': 'lower'},
    'MMAP_FULL': {'label': 'mmap Access 100%', 'category': 'mmap', 'objective': 'lower'},
    'SCHED_THROUGHPUT_RR': {'label': 'Throughput RR', 'category': 'scheduler', 'objective': 'lower'},
    'SCHED_THROUGHPUT_FCFS': {'label': 'Throughput FCFS', 'category': 'scheduler', 'objective': 'lower'},
    'SCHED_THROUGHPUT_SJF': {'label': 'Throughput SJF', 'category': 'scheduler', 'objective': 'lower'},
    'SCHED_CONVOY_RR': {'label': 'Convoy RR', 'category': 'scheduler', 'objective': 'lower'},
    'SCHED_CONVOY_FCFS': {'label': 'Convoy FCFS', 'category': 'scheduler', 'objective': 'lower'},
    'SCHED_CONVOY_SRTF': {'label': 'Convoy SRTF', 'category': 'scheduler', 'objective': 'lower'},
    'SCHED_FAIRNESS_RR': {'label': 'Fairness RR', 'category': 'scheduler', 'objective': 'lower'},
    'SCHED_FAIRNESS_PRIORITY': {'label': 'Fairness PRIORITY', 'category': 'scheduler', 'objective': 'lower'},
    'SCHED_FAIRNESS_CFS': {'label': 'Fairness CFS', 'category': 'scheduler', 'objective': 'lower'},
    'SCHED_RESPONSE_RR': {'label': 'Response RR', 'category': 'scheduler', 'objective': 'lower'},
    'SCHED_RESPONSE_MLFQ': {'label': 'Response MLFQ', 'category': 'scheduler', 'objective': 'lower'},
}

MEMORY_COMPARISON_TESTS = [
    'COW_NO_ACCESS',
    'COW_READONLY',
    'COW_PARTIAL',
    'COW_FULLWRITE',
    'LAZY_SPARSE',
    'LAZY_HALF',
    'LAZY_FULL',
]

META_PATTERN = re.compile(r'META:ts=(\d+):cpus=(\d+):branch=(\w+):sched=(\w+)')
RESULT_PATTERN = re.compile(r'RESULT:(\w+):(-?\d+(?:\.\d+)?):(\d+(?:\.\d+)?):(\w+)(?::(\d+):(\d+))?')
SAMPLES_PATTERN = re.compile(r'SAMPLES:(\w+):(\d+):([0-9,.\-]+)')


def _mean(values):
    return sum(values) / len(values) if values else 0.0


def _sample_std(values):
    if len(values) < 2:
        return 0.0
    avg = _mean(values)
    variance = sum((value - avg) ** 2 for value in values) / (len(values) - 1)
    return math.sqrt(variance)


def result_is_valid(result):
    return bool(result) and result.get('avg') is not None and result.get('unit') != 'error' and not result.get('consistency_error')


def result_signature(result):
    return (
        result.get('unit', ''),
        result.get('batch', 1),
        result.get('runs', 1),
    )


def describe_signature(signature, run_ids=None):
    unit, batch, runs = signature
    detail = f'unit={unit}, batch={batch}, runs={runs}'
    if run_ids:
        detail = f'{detail} [{", ".join(run_ids)}]'
    return detail


def parse_log_content(content):
    results = {}
    metadata = {}

    for line in content.splitlines():
        meta_match = META_PATTERN.match(line)
        if meta_match:
            ts, cpus, branch, sched = meta_match.groups()
            metadata = {
                'timestamp': int(ts),
                'cpus': int(cpus),
                'branch': branch,
                'sched': sched,
            }
            continue

        result_match = RESULT_PATTERN.match(line)
        if result_match:
            name, avg, std, unit, batch, runs = result_match.groups()
            entry = results.setdefault(name, {})
            entry.update(
                {
                    'avg': float(avg),
                    'std': float(std),
                    'unit': unit,
                    'batch': int(batch) if batch else 1,
                    'runs': int(runs) if runs else 1,
                }
            )
            continue

        sample_match = SAMPLES_PATTERN.match(line)
        if sample_match:
            name, _count, values = sample_match.groups()
            entry = results.setdefault(name, {})
            entry['samples'] = [float(item) for item in values.split(',') if item]

    return {'metadata': metadata, 'results': results}


def sanitize_log_content(content):
    lines = content.splitlines()
    captured = []
    capture = False
    completed = False
    separators = 0

    for line in lines:
        if '$ perftest ' in line:
            capture = True
        if capture:
            captured.append(line)
        if capture and 'Tests Complete' in line:
            completed = True
        if completed and line.strip() == '========================================':
            separators += 1
            if separators >= 2:
                break

    sanitized = '\n'.join(captured)
    if 'RESULT:' not in sanitized:
        sanitized = '\n'.join(line for line in lines if line.startswith(('META:', 'RESULT:', 'SAMPLES:')))
    if 'RESULT:' not in sanitized:
        return content
    return sanitized


def parse_log_file(filepath):
    path = Path(filepath)
    if not path.exists():
        return {'metadata': {}, 'results': {}, 'error': 'File not found'}
    content = path.read_text(encoding='utf-8', errors='ignore')
    parsed = parse_log_content(content)
    parsed['error'] = None
    return parsed


def log_file_info(filepath):
    path = Path(filepath)
    if not path.exists():
        return {
            'path': str(path),
            'exists': False,
            'size_bytes': 0,
            'lines': 0,
            'sha256': None,
            'modified_at': None,
        }

    content = path.read_bytes()
    return {
        'path': str(path),
        'exists': True,
        'size_bytes': len(content),
        'lines': content.count(b'\n'),
        'sha256': hashlib.sha256(content).hexdigest(),
        'modified_at': path.stat().st_mtime,
    }


def discover_run_logs(branch_dir):
    branch_dir = Path(branch_dir)
    runs_dir = branch_dir / 'runs'
    if runs_dir.exists():
        run_logs = {}
        for path in sorted(runs_dir.glob('run*_clean.log')):
            run_id = re.sub(r'_(clean|raw)$', '', path.stem)
            run_logs[run_id] = path
        for path in sorted(runs_dir.glob('run*_raw.log')):
            run_id = re.sub(r'_(clean|raw)$', '', path.stem)
            run_logs.setdefault(run_id, path)
        if run_logs:
            return [run_logs[run_id] for run_id in sorted(run_logs)]

    legacy_log = branch_dir / 'perftest.log'
    if legacy_log.exists():
        return [legacy_log]
    return []


def load_manifest(manifest_path):
    path = Path(manifest_path)
    if not path.exists():
        return []

    with path.open('r', encoding='utf-8', newline='') as handle:
        reader = csv.DictReader(handle, delimiter='\t')
        return list(reader)


def _run_id_from_path(path):
    if path.stem == 'perftest':
        return 'legacy_01'
    return re.sub(r'_(clean|raw)$', '', path.stem)


def _source_kind(path):
    if path.name.endswith('_clean.log'):
        return 'clean'
    if path.name.endswith('_raw.log'):
        return 'raw'
    return 'legacy'


def aggregate_results(run_entries):
    all_names = sorted({name for run in run_entries for name in run['results'].keys()})
    aggregated = {}
    total_rounds = len(run_entries)
    aggregation_errors = []

    for test_name in all_names:
        valid_values = []
        inner_stds = []
        batch = 1
        runs = 1
        unit = ''
        round_rows = []
        sample_values = []
        per_run_samples = []
        invalid_rounds = 0
        signatures = {}

        for run in run_entries:
            raw = run['results'].get(test_name)
            if not raw:
                continue

            signature = result_signature(raw)
            signatures.setdefault(signature, []).append(run['run_id'])

            round_rows.append(
                {
                    'run_id': run['run_id'],
                    'path': run['source']['path'],
                    'status': run['summary']['status'],
                    'source_kind': run['source']['kind'],
                    'manifest_commit': run['manifest_row'].get('commit', ''),
                    'manifest_command': run['manifest_row'].get('command', ''),
                    'avg': raw['avg'],
                    'std': raw['std'],
                    'unit': raw['unit'],
                    'batch': raw.get('batch', 1),
                    'runs': raw.get('runs', 1),
                }
            )

            run_samples = raw.get('samples', [])
            if run_samples:
                rounded_samples = [round(value, 2) for value in run_samples]
                sample_values.extend(run_samples)
                per_run_samples.append(
                    {
                        'run_id': run['run_id'],
                        'path': run['source']['path'],
                        'count': len(rounded_samples),
                        'values': rounded_samples,
                    }
                )
            batch = raw.get('batch', batch)
            runs = raw.get('runs', runs)
            unit = raw.get('unit', unit)

            if raw['avg'] >= 0 and raw['unit'] != 'error':
                valid_values.append(raw['avg'])
                inner_stds.append(raw['std'])
            else:
                invalid_rounds += 1

        configurations = [
            {
                'unit': signature[0],
                'batch': signature[1],
                'runs': signature[2],
                'run_ids': run_ids,
            }
            for signature, run_ids in sorted(signatures.items())
        ]

        sample_avg = round(_mean(sample_values), 2) if sample_values else None
        sample_std = round(_sample_std(sample_values), 2) if len(sample_values) >= 2 else 0.0
        sample_ci95 = round(1.96 * _sample_std(sample_values) / math.sqrt(len(sample_values)), 2) if len(sample_values) >= 2 else 0.0

        if len(signatures) > 1:
            message = 'Mixed measurement signatures across runs: ' + '; '.join(
                describe_signature(signature, run_ids) for signature, run_ids in sorted(signatures.items())
            )
            aggregated[test_name] = {
                'avg': None,
                'std': None,
                'unit': None,
                'batch': None,
                'runs': None,
                'round_count': len(valid_values),
                'rounds_total': total_rounds,
                'rounds_present': len(round_rows),
                'invalid_rounds': invalid_rounds,
                'min': None,
                'max': None,
                'ci95': None,
                'inner_std_avg': round(_mean(inner_stds), 2) if inner_stds else None,
                'round_cv_percent': None,
                'values': [round(value, 2) for value in valid_values],
                'per_run': round_rows,
                'sample_count': len(sample_values),
                'sample_avg': sample_avg,
                'sample_std': sample_std,
                'sample_ci95': sample_ci95,
                'sample_min': round(min(sample_values), 2) if sample_values else None,
                'sample_max': round(max(sample_values), 2) if sample_values else None,
                'sample_values': [round(value, 2) for value in sample_values],
                'per_run_samples': per_run_samples,
                'consistency_error': message,
                'configurations': configurations,
            }
            aggregation_errors.append({'test': test_name, 'message': message, 'configurations': configurations})
            continue

        if valid_values:
            round_std = _sample_std(valid_values)
            ci95 = 1.96 * round_std / math.sqrt(len(valid_values)) if len(valid_values) >= 2 else 0.0
            aggregated[test_name] = {
                'avg': round(_mean(valid_values), 2),
                'std': round(round_std if len(valid_values) >= 2 else _mean(inner_stds), 2),
                'unit': unit,
                'batch': batch,
                'runs': runs,
                'round_count': len(valid_values),
                'rounds_total': total_rounds,
                'rounds_present': len(round_rows),
                'invalid_rounds': invalid_rounds,
                'min': round(min(valid_values), 2),
                'max': round(max(valid_values), 2),
                'ci95': round(ci95, 2),
                'inner_std_avg': round(_mean(inner_stds), 2),
                'round_cv_percent': round((round_std * 100.0 / _mean(valid_values)), 2) if len(valid_values) >= 2 and _mean(valid_values) > 0 else None,
                'values': [round(value, 2) for value in valid_values],
                'per_run': round_rows,
                'sample_count': len(sample_values),
                'sample_avg': sample_avg,
                'sample_std': sample_std,
                'sample_ci95': sample_ci95,
                'sample_min': round(min(sample_values), 2) if sample_values else None,
                'sample_max': round(max(sample_values), 2) if sample_values else None,
                'sample_values': [round(value, 2) for value in sample_values],
                'per_run_samples': per_run_samples,
                'consistency_error': None,
                'configurations': configurations,
            }
            continue

        fallback = round_rows[-1] if round_rows else None
        if fallback:
            aggregated[test_name] = {
                'avg': fallback['avg'],
                'std': fallback['std'],
                'unit': fallback['unit'],
                'batch': fallback['batch'],
                'runs': fallback['runs'],
                'round_count': 0,
                'rounds_total': total_rounds,
                'rounds_present': len(round_rows),
                'invalid_rounds': invalid_rounds,
                'min': None,
                'max': None,
                'ci95': None,
                'inner_std_avg': None,
                'round_cv_percent': None,
                'values': [],
                'per_run': round_rows,
                'sample_count': len(sample_values),
                'sample_avg': sample_avg,
                'sample_std': sample_std,
                'sample_ci95': sample_ci95,
                'sample_min': round(min(sample_values), 2) if sample_values else None,
                'sample_max': round(max(sample_values), 2) if sample_values else None,
                'sample_values': [round(value, 2) for value in sample_values],
                'per_run_samples': per_run_samples,
                'consistency_error': None,
                'configurations': configurations,
            }

    return aggregated, aggregation_errors


def required_tests_for(branch_name):
    return TESTING_REQUIRED if branch_name == 'testing' else COMMON_REQUIRED


def _run_summary(branch_name, run_id, parsed, source, manifest_row):
    required = required_tests_for(branch_name)
    missing_required = [name for name in required if name not in parsed['results']]
    present_required = len(required) - len(missing_required)

    if not parsed['results']:
        status = 'empty'
    elif missing_required:
        status = 'partial'
    else:
        status = 'complete'

    return {
        'run_id': run_id,
        'status': status,
        'source_kind': source['kind'],
        'path': source['path'],
        'required_present': present_required,
        'required_total': len(required),
        'missing_required': missing_required,
        'manifest': manifest_row or {},
    }


def summarize_collection(run_entries, manifest_rows):
    commits = sorted({row.get('commit') for row in manifest_rows if row.get('commit')})
    commands = sorted({row.get('command') for row in manifest_rows if row.get('command')})
    cpus = sorted({str(row.get('cpus')) for row in manifest_rows if row.get('cpus') not in (None, '')})

    return {
        'runs_total': len(run_entries),
        'complete_runs': sum(1 for run in run_entries if run['summary']['status'] == 'complete'),
        'partial_runs': sum(1 for run in run_entries if run['summary']['status'] == 'partial'),
        'empty_runs': sum(1 for run in run_entries if run['summary']['status'] == 'empty'),
        'raw_only_runs': sum(1 for run in run_entries if run['source']['kind'] == 'raw'),
        'manifest_rows': len(manifest_rows),
        'unique_commits': commits,
        'unique_commands': commands,
        'unique_cpus': cpus,
    }


def load_branch_dataset(branch_name, log_dir=LOG_DIR):
    branch_dir = Path(log_dir) / branch_name
    manifest_rows = load_manifest(branch_dir / 'manifest.tsv')
    manifest_by_run = {row.get('run_id'): row for row in manifest_rows if row.get('run_id')}
    run_paths = discover_run_logs(branch_dir)
    run_entries = []

    for path in run_paths:
        source_kind = _source_kind(path)
        content = path.read_text(encoding='utf-8', errors='ignore')
        if source_kind == 'raw':
            content = sanitize_log_content(content)
        parsed = parse_log_content(content)
        run_id = _run_id_from_path(path)
        source = log_file_info(path)
        source['kind'] = source_kind
        source['sanitized_from_raw'] = source_kind == 'raw'
        manifest_row = manifest_by_run.get(run_id)
        run_entries.append(
            {
                'run_id': run_id,
                'metadata': parsed['metadata'],
                'results': parsed['results'],
                'error': None,
                'source': source,
                'summary': _run_summary(branch_name, run_id, parsed, source, manifest_row),
                'manifest_row': manifest_row or {},
            }
        )

    aggregate, aggregation_errors = aggregate_results(run_entries)
    latest_meta = {}
    for run in reversed(run_entries):
        if run['metadata']:
            latest_meta = run['metadata']
            break

    return {
        'branch': branch_name,
        'metadata': latest_meta,
        'results': aggregate,
        'runs': run_entries,
        'manifest': manifest_rows,
        'run_statuses': [run['summary'] for run in run_entries],
        'collection_summary': summarize_collection(run_entries, manifest_rows),
        'aggregation_errors': aggregation_errors,
        'source_files': [run['source'] for run in run_entries],
        'branch_dir': str(branch_dir),
        'legacy_log': log_file_info(branch_dir / 'perftest.log'),
    }
