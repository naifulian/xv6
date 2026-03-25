#!/usr/bin/env python3
"""Parse xv6 experiment logs and generate thesis-ready figures, tables, and reports."""

import csv
import json
from datetime import datetime
from pathlib import Path

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError:
    print('[ERROR] matplotlib or numpy is not installed')
    raise SystemExit(1)

from experiment_data import (
    COMMON_REQUIRED,
    MEMORY_COMPARISON_TESTS,
    SCENARIOS,
    TESTING_REQUIRED,
    TEST_META,
    load_branch_dataset,
    result_is_valid,
)

EXPERIMENTS_DIR = Path(__file__).resolve().parents[1]
OUTPUTS_DIR = EXPERIMENTS_DIR / 'outputs'
OUTPUT_DIR = OUTPUTS_DIR / 'figures'
DATA_DIR = OUTPUTS_DIR / 'data'
REPORT_DIR = OUTPUTS_DIR / 'reports'


def ensure_output_dirs():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    REPORT_DIR.mkdir(parents=True, exist_ok=True)


def setup_style():
    try:
        plt.style.use('seaborn-v0_8-darkgrid')
    except Exception:
        pass
    plt.rcParams['figure.dpi'] = 300
    plt.rcParams['font.size'] = 11
    plt.rcParams['axes.unicode_minus'] = False


def save_figure(fig, fig_id):
    ensure_output_dirs()
    fig.savefig(OUTPUT_DIR / f'{fig_id}.png', dpi=300, bbox_inches='tight')
    fig.savefig(OUTPUT_DIR / f'{fig_id}.pdf', format='pdf', bbox_inches='tight')
    plt.close(fig)


def entry(results, name):
    return results.get(name, {})


def is_valid(result):
    return result_is_valid(result)


def metric_value(results, name):
    item = entry(results, name)
    return item.get('avg') if is_valid(item) else None


def avg(results, name):
    value = metric_value(results, name)
    return value if value is not None else 0


def error_bar(results, name):
    item = entry(results, name)
    if not is_valid(item):
        return 0
    if item.get('round_count', 0) >= 2 and item.get('ci95') is not None:
        return item.get('ci95', 0)
    return item.get('std', 0)


def round_count(results, name):
    item = entry(results, name)
    return item.get('round_count', 0)


def runs(results, name):
    item = entry(results, name)
    return item.get('runs', 1) if item else 1


def cv_percent(result):
    if not is_valid(result) or result.get('avg', 0) <= 0:
        return None
    if result.get('round_cv_percent') is not None:
        return result.get('round_cv_percent')
    return result.get('std', 0) * 100.0 / result.get('avg', 1)


def per_batch_avg(result):
    if not is_valid(result) or result.get('batch', 0) <= 0:
        return None
    return result['avg'] / result['batch']


def improvement(baseline, testing, test_name, bigger_is_better=False):
    base = entry(baseline, test_name)
    test = entry(testing, test_name)
    if not is_valid(base) or not is_valid(test) or base['avg'] == 0:
        return None
    if bigger_is_better:
        return (test['avg'] - base['avg']) * 100.0 / base['avg']
    return (base['avg'] - test['avg']) * 100.0 / base['avg']


def mean_positive(values):
    values = [value for value in values if value is not None]
    return sum(values) / len(values) if values else 0.0


def format_pct(value):
    return 'N/A' if value is None else f'{value:.1f}%'


def format_value(value):
    if value is None:
        return 'N/A'
    if abs(value - round(value)) < 0.01:
        return str(int(round(value)))
    return f'{value:.2f}'


def low_resolution_runs(result):
    flagged = []
    if not result:
        return flagged

    for row in result.get('per_run', []):
        if row.get('status') != 'complete':
            continue
        if row.get('unit') != 'ticks':
            continue
        if row.get('avg', 0) > 3 or row.get('std', 0) != 0:
            continue
        if row.get('runs', 0) < 5:
            continue
        flagged.append(row)

    return flagged


def quality_flags(name, result):
    flags = []
    if result.get('consistency_error'):
        flags.append('mixed_signature')
    if not is_valid(result):
        flags.append('invalid')
        return flags
    if low_resolution_runs(result):
        flags.append('low_resolution')
    value_cv = cv_percent(result)
    if value_cv is not None and value_cv >= 10.0:
        flags.append('high_variation')
    if result.get('round_count', 0) >= 3:
        flags.append('repeatable')
    elif result.get('round_count', 0) == 2:
        flags.append('two_rounds')
    else:
        flags.append('single_round')
    if name.startswith('SCHED_'):
        flags.append('scheduler')
    return flags


def comparison_comment(delta, relation):
    if relation == 'invalid-dataset':
        return 'invalid mixed dataset'
    if relation == 'insufficient-rounds':
        return 'trend only; add complete rounds'
    if relation == 'overlap':
        return 'difference not clearly separated'
    if delta is None:
        return 'N/A'
    if delta >= 20:
        return 'significant improvement'
    if delta >= 5:
        return 'moderate improvement'
    if delta > -5:
        return 'similar performance'
    if delta > -20:
        return 'moderate regression'
    return 'clear counter-example'


def interval_relation(base_result, test_result):
    if (base_result or {}).get('consistency_error') or (test_result or {}).get('consistency_error'):
        return 'invalid-dataset'
    if not is_valid(base_result) or not is_valid(test_result):
        return 'n/a'
    if base_result.get('round_count', 0) < 2 or test_result.get('round_count', 0) < 2:
        return 'insufficient-rounds'
    base_err = base_result.get('ci95')
    test_err = test_result.get('ci95')
    if base_err is None or test_err is None:
        return 'insufficient-rounds'
    base_low = base_result['avg'] - base_err
    base_high = base_result['avg'] + base_err
    test_low = test_result['avg'] - test_err
    test_high = test_result['avg'] + test_err
    return 'separated' if base_high < test_low or test_high < base_low else 'overlap'


def plot_cow_comparison(baseline, testing):
    tests = ['COW_NO_ACCESS', 'COW_READONLY', 'COW_PARTIAL', 'COW_FULLWRITE']
    labels = ['No Access', 'Read Only', 'Write 30%', 'Write 100%']
    base_vals = [avg(baseline, test) for test in tests]
    test_vals = [avg(testing, test) for test in tests]
    base_errs = [error_bar(baseline, test) for test in tests]
    test_errs = [error_bar(testing, test) for test in tests]
    if not any(base_vals + test_vals):
        return

    fig, ax = plt.subplots(figsize=(10, 6))
    x = np.arange(len(labels))
    ax.errorbar(x, base_vals, yerr=base_errs, fmt='o-', linewidth=2, capsize=4, label='Baseline (Eager Copy)', color='#2563eb')
    ax.errorbar(x, test_vals, yerr=test_errs, fmt='s-', linewidth=2, capsize=4, label='Testing (COW)', color='#059669')
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_xlabel('Scenario', fontweight='bold')
    ax.set_ylabel('Time (ticks)', fontweight='bold')
    ax.set_title('Copy-on-Write Performance Comparison', fontweight='bold')
    ax.legend()
    save_figure(fig, 'fig1_cow_comparison')


def plot_lazy_comparison(baseline, testing):
    tests = ['LAZY_SPARSE', 'LAZY_HALF', 'LAZY_FULL']
    labels = ['Access 1%', 'Access 50%', 'Access 100%']
    base_vals = [avg(baseline, test) for test in tests]
    test_vals = [avg(testing, test) for test in tests]
    base_errs = [error_bar(baseline, test) for test in tests]
    test_errs = [error_bar(testing, test) for test in tests]
    if not any(base_vals + test_vals):
        return

    fig, ax = plt.subplots(figsize=(10, 6))
    x = np.arange(len(labels))
    ax.errorbar(x, base_vals, yerr=base_errs, fmt='o-', linewidth=2, capsize=4, label='Baseline (Eager Alloc)', color='#2563eb')
    ax.errorbar(x, test_vals, yerr=test_errs, fmt='s-', linewidth=2, capsize=4, label='Testing (Lazy Alloc)', color='#059669')
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_xlabel('Access Ratio', fontweight='bold')
    ax.set_ylabel('Time (ticks)', fontweight='bold')
    ax.set_title('Lazy Allocation Performance Comparison', fontweight='bold')
    ax.legend()
    save_figure(fig, 'fig2_lazy_comparison')


def plot_memory_overall(baseline, testing):
    tests = ['COW_NO_ACCESS', 'COW_READONLY', 'COW_PARTIAL', 'COW_FULLWRITE', 'LAZY_SPARSE', 'LAZY_HALF', 'LAZY_FULL']
    labels = ['COW-No Access', 'COW-Read', 'COW-30%', 'COW-100%', 'Lazy-1%', 'Lazy-50%', 'Lazy-100%']
    base_vals = [avg(baseline, test) for test in tests]
    test_vals = [avg(testing, test) for test in tests]
    base_errs = [error_bar(baseline, test) for test in tests]
    test_errs = [error_bar(testing, test) for test in tests]
    if not any(base_vals + test_vals):
        return

    x = np.arange(len(labels))
    width = 0.38
    fig, ax = plt.subplots(figsize=(13, 6))
    ax.bar(x - width / 2, base_vals, width, yerr=base_errs, capsize=4, label='Baseline', color='#2563eb')
    ax.bar(x + width / 2, test_vals, width, yerr=test_errs, capsize=4, label='Testing', color='#059669')
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=20, ha='right')
    ax.set_ylabel('Time (ticks)', fontweight='bold')
    ax.set_title('Memory Optimization Overview', fontweight='bold')
    ax.legend()
    save_figure(fig, 'fig4_memory_overall')


def plot_scheduler_comparison(testing):
    tests = ['SCHED_THROUGHPUT_RR', 'SCHED_THROUGHPUT_FCFS', 'SCHED_THROUGHPUT_SJF']
    labels = ['RR', 'FCFS', 'SJF']
    vals = [avg(testing, test) for test in tests]
    errs = [error_bar(testing, test) for test in tests]
    if not any(vals):
        return

    fig, ax = plt.subplots(figsize=(8, 6))
    bars = ax.bar(labels, vals, yerr=errs, capsize=4, color=['#2563eb', '#0891b2', '#dc2626'])
    ax.set_ylabel('Completion Time (ticks)', fontweight='bold')
    ax.set_title('Scheduler Throughput Comparison', fontweight='bold')
    for bar, value in zip(bars, vals):
        ax.text(bar.get_x() + bar.get_width() / 2, value, format_value(value), ha='center', va='bottom', fontweight='bold')
    save_figure(fig, 'fig5_scheduler_throughput')


def plot_scheduler_scenarios(testing):
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    axes = axes.flatten()
    has_data = False
    palette = ['#2563eb', '#059669', '#dc2626']

    for idx, (scenario, tests) in enumerate(SCENARIOS.items()):
        labels = [test.split('_')[-1] for test in tests]
        vals = [avg(testing, test) for test in tests]
        errs = [error_bar(testing, test) for test in tests]
        ax = axes[idx]
        if any(vals):
            has_data = True
            ax.bar(labels, vals, yerr=errs, capsize=4, color=palette[:len(labels)])
            ax.set_title(scenario.title(), fontweight='bold')
            ax.set_ylabel('CPU-time spread (ticks)' if scenario == 'fairness' else 'Time (ticks)')
        else:
            ax.text(0.5, 0.5, 'No Data', ha='center', va='center', transform=ax.transAxes)
            ax.set_title(f'{scenario.title()} (No Data)', fontweight='bold')

    if has_data:
        fig.suptitle('Scheduler Scenario Comparison', fontweight='bold')
        fig.tight_layout()
        save_figure(fig, 'fig6_scheduler_scenarios')
    else:
        plt.close(fig)


def compute_scheduler_spread(testing):
    scores = []
    for tests in SCENARIOS.values():
        vals = [avg(testing, test) for test in tests if avg(testing, test) > 0]
        if len(vals) >= 2 and max(vals) > 0:
            scores.append((max(vals) - min(vals)) * 100.0 / max(vals))
    return sum(scores) / len(scores) if scores else 0.0


def plot_overall_radar(baseline, testing):
    valid_baseline = sum(1 for name in COMMON_REQUIRED if is_valid(entry(baseline, name)))
    valid_testing = sum(1 for name in TESTING_REQUIRED if is_valid(entry(testing, name)))
    completeness = (valid_baseline + valid_testing) * 100.0 / (len(COMMON_REQUIRED) + len(TESTING_REQUIRED))
    categories = ['COW', 'Lazy', 'Scheduler', 'Completeness']
    values = [
        min(100.0, max(0.0, mean_positive([improvement(baseline, testing, name) for name in ['COW_NO_ACCESS', 'COW_READONLY', 'COW_PARTIAL']]))),
        min(100.0, max(0.0, mean_positive([improvement(baseline, testing, name) for name in ['LAZY_SPARSE', 'LAZY_HALF']]))),
        min(100.0, max(0.0, compute_scheduler_spread(testing))),
        min(100.0, max(0.0, completeness)),
    ]
    if not any(values):
        return

    angles = np.linspace(0, 2 * np.pi, len(categories), endpoint=False).tolist()
    angles += angles[:1]
    values += values[:1]
    fig, ax = plt.subplots(figsize=(9, 9), subplot_kw={'polar': True})
    ax.plot(angles, values, color='#2563eb', linewidth=2, marker='o')
    ax.fill(angles, values, color='#60a5fa', alpha=0.25)
    ax.set_xticks(angles[:-1])
    ax.set_xticklabels(categories)
    ax.set_ylim(0, 100)
    ax.set_title('Experiment Quality Radar', fontweight='bold', y=1.08)
    save_figure(fig, 'fig7_overall_radar')


def plot_memory_speedups(baseline, testing):
    labels = []
    values = []
    for test_name in MEMORY_COMPARISON_TESTS:
        meta = TEST_META[test_name]
        delta = improvement(baseline, testing, test_name, bigger_is_better=(meta['objective'] == 'higher'))
        if delta is None:
            continue
        labels.append(meta['label'])
        values.append(delta)
    if not values:
        return

    colors = ['#059669' if value >= 0 else '#dc2626' for value in values]
    fig, ax = plt.subplots(figsize=(10, 6))
    y = np.arange(len(labels))
    ax.barh(y, values, color=colors)
    ax.axvline(0, color='#111827', linewidth=1)
    ax.set_yticks(y)
    ax.set_yticklabels(labels)
    ax.set_xlabel('Improvement vs. Baseline (%)', fontweight='bold')
    ax.set_title('Memory Optimization Speedup / Slowdown Profile', fontweight='bold')
    for idx, value in enumerate(values):
        align = 'left' if value >= 0 else 'right'
        offset = 1.0 if value >= 0 else -1.0
        ax.text(value + offset, idx, f'{value:.1f}%', va='center', ha=align, fontweight='bold')
    save_figure(fig, 'fig8_memory_speedups')


def plot_variation_profile(baseline, testing):
    tests = [name for name in COMMON_REQUIRED if name not in ('SYS_TOTAL_PAGES', 'SYS_FREE_PAGES')]
    labels = [TEST_META[name]['label'] for name in tests]
    base_vals = [cv_percent(entry(baseline, name)) or 0 for name in tests]
    test_vals = [cv_percent(entry(testing, name)) or 0 for name in tests]
    if not any(base_vals + test_vals):
        return

    x = np.arange(len(labels))
    width = 0.38
    fig, ax = plt.subplots(figsize=(13, 6))
    ax.bar(x - width / 2, base_vals, width, label='Baseline', color='#2563eb')
    ax.bar(x + width / 2, test_vals, width, label='Testing', color='#059669')
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=25, ha='right')
    ax.set_ylabel('Variation (CV %)', fontweight='bold')
    ax.set_title('Measurement Stability Profile', fontweight='bold')
    ax.legend()
    save_figure(fig, 'fig9_variation_profile')


def compute_scheduler_analysis(testing):
    analysis = {}
    for scenario, tests in SCENARIOS.items():
        rows = []
        valid_values = []
        for test_name in tests:
            current = entry(testing, test_name)
            value = avg(testing, test_name)
            row = {
                'test': test_name,
                'algorithm': test_name.split('_')[-1],
                'avg': value,
                'error_bar': error_bar(testing, test_name),
                'std': current.get('std', 0),
                'unit': current.get('unit', 'ticks'),
                'round_count': current.get('round_count', 0),
            }
            rows.append(row)
            if value > 0:
                valid_values.append(value)
        ranked = sorted([row for row in rows if row['avg'] > 0], key=lambda item: item['avg'])
        for idx, row in enumerate(ranked, start=1):
            row['rank'] = idx
        spread = 0.0
        if len(valid_values) >= 2 and max(valid_values) > 0:
            spread = (max(valid_values) - min(valid_values)) * 100.0 / max(valid_values)
        analysis[scenario] = {
            'rows': rows,
            'winner': ranked[0]['algorithm'] if ranked else None,
            'spread_percent': spread,
        }
    return analysis


def compute_quality_report(baseline_data, testing_data):
    baseline = baseline_data['results']
    testing = testing_data['results']
    tracked_tests = set(COMMON_REQUIRED) | set(TESTING_REQUIRED)
    low_resolution = []
    high_variation = []
    counterexamples = []
    aggregation_errors = []

    for branch_name, dataset, results in (('baseline', baseline_data, baseline), ('testing', testing_data, testing)):
        for item in dataset.get('aggregation_errors', []):
            if item['test'] in tracked_tests:
                aggregation_errors.append({'branch': branch_name, 'test': item['test'], 'message': item['message']})
        for name, result in results.items():
            if name not in tracked_tests:
                continue
            flags = quality_flags(name, result)
            for row in low_resolution_runs(result):
                low_resolution.append({
                    'branch': branch_name,
                    'test': name,
                    'run_id': row.get('run_id', ''),
                    'avg': row.get('avg'),
                    'std': row.get('std'),
                    'batch': row.get('batch'),
                })
            if 'high_variation' in flags:
                high_variation.append({'branch': branch_name, 'test': name, 'cv_percent': round(cv_percent(result), 2)})

    for test_name in MEMORY_COMPARISON_TESTS:
        meta = TEST_META[test_name]
        delta = improvement(baseline, testing, test_name, bigger_is_better=(meta['objective'] == 'higher'))
        if delta is not None and delta < 0:
            counterexamples.append({'test': test_name, 'label': meta['label'], 'delta_percent': round(delta, 2)})

    coverage = (
        sum(1 for name in COMMON_REQUIRED if is_valid(entry(baseline, name))) +
        sum(1 for name in TESTING_REQUIRED if is_valid(entry(testing, name)))
    ) * 100.0 / (len(COMMON_REQUIRED) + len(TESTING_REQUIRED))

    repeatable_items = (
        sum(1 for name in COMMON_REQUIRED if entry(baseline, name).get('round_count', 0) >= 3) +
        sum(1 for name in TESTING_REQUIRED if entry(testing, name).get('round_count', 0) >= 3)
    )
    repeatability = repeatable_items * 100.0 / (len(COMMON_REQUIRED) + len(TESTING_REQUIRED))
    scheduler_spread = compute_scheduler_spread(testing)
    stability_score = max(0.0, 100.0 - min(100.0, len(low_resolution) * 8.0 + len(high_variation) * 5.0 + len(aggregation_errors) * 12.0))
    readiness_score = round((0.35 * coverage) + (0.25 * repeatability) + (0.20 * scheduler_spread) + (0.20 * stability_score), 2)

    return {
        'coverage_percent': round(coverage, 2),
        'repeatability_percent': round(repeatability, 2),
        'scheduler_spread_score': round(scheduler_spread, 2),
        'stability_score': round(stability_score, 2),
        'readiness_score': readiness_score,
        'baseline_rounds_detected': len(baseline_data['runs']),
        'testing_rounds_detected': len(testing_data['runs']),
        'low_resolution_tests': low_resolution,
        'high_variation_tests': high_variation,
        'aggregation_errors': aggregation_errors,
        'counterexamples': counterexamples,
    }


def compute_summary(baseline_data, testing_data):
    baseline = baseline_data['results']
    testing = testing_data['results']
    return {
        'baseline_valid': sum(1 for name in COMMON_REQUIRED if is_valid(entry(baseline, name))),
        'baseline_expected': len(COMMON_REQUIRED),
        'baseline_rounds': len(baseline_data['runs']),
        'testing_valid': sum(1 for name in TESTING_REQUIRED if is_valid(entry(testing, name))),
        'testing_expected': len(TESTING_REQUIRED),
        'testing_rounds': len(testing_data['runs']),
        'improvements': {
            'cow_best_case': improvement(baseline, testing, 'COW_NO_ACCESS'),
            'cow_readonly': improvement(baseline, testing, 'COW_READONLY'),
            'cow_partial': improvement(baseline, testing, 'COW_PARTIAL'),
            'cow_fullwrite': improvement(baseline, testing, 'COW_FULLWRITE'),
            'lazy_sparse': improvement(baseline, testing, 'LAZY_SPARSE'),
            'lazy_half': improvement(baseline, testing, 'LAZY_HALF'),
            'lazy_full': improvement(baseline, testing, 'LAZY_FULL'),
        },
        'scheduler_spread_score': compute_scheduler_spread(testing),
    }


def build_long_rows(baseline_data, testing_data):
    rows = []
    for branch_name, dataset in (('baseline', baseline_data), ('testing', testing_data)):
        for test_name, result in sorted(dataset['results'].items()):
            meta = TEST_META.get(test_name, {'label': test_name, 'category': 'other', 'objective': 'lower'})
            rows.append({
                'branch': branch_name,
                'test': test_name,
                'label': meta['label'],
                'category': meta['category'],
                'objective': meta['objective'],
                'avg': result['avg'],
                'error_bar': result.get('ci95') if result.get('round_count', 0) >= 2 else result.get('std', 0),
                'std': result.get('std', 0),
                'ci95': result.get('ci95', ''),
                'unit': result.get('unit', ''),
                'batch': result.get('batch', 1),
                'runs': result.get('runs', 1),
                'round_count': result.get('round_count', 0),
                'rounds_present': result.get('rounds_present', 0),
                'rounds_total': result.get('rounds_total', 0),
                'min': result.get('min', ''),
                'max': result.get('max', ''),
                'sample_count': result.get('sample_count', 0),
                'sample_avg': result.get('sample_avg', ''),
                'sample_std': result.get('sample_std', ''),
                'sample_ci95': result.get('sample_ci95', ''),
                'sample_min': result.get('sample_min', ''),
                'sample_max': result.get('sample_max', ''),
                'avg_per_batch': '' if per_batch_avg(result) is None else round(per_batch_avg(result), 4),
                'cv_percent': '' if cv_percent(result) is None else round(cv_percent(result), 2),
                'inner_std_avg': result.get('inner_std_avg', ''),
                'consistency_error': result.get('consistency_error', ''),
                'quality_flags': ','.join(quality_flags(test_name, result)),
            })
    return rows


def build_round_rows(baseline_data, testing_data):
    rows = []
    for branch_name, dataset in (('baseline', baseline_data), ('testing', testing_data)):
        for run in dataset['runs']:
            for test_name, result in sorted(run['results'].items()):
                meta = TEST_META.get(test_name, {'label': test_name, 'category': 'other', 'objective': 'lower'})
                rows.append({
                    'branch': branch_name,
                    'run_id': run['run_id'],
                    'run_status': run['summary']['status'],
                    'source_kind': run['source']['kind'],
                    'manifest_commit': run['manifest_row'].get('commit', ''),
                    'manifest_command': run['manifest_row'].get('command', ''),
                    'source_path': run['source']['path'],
                    'test': test_name,
                    'label': meta['label'],
                    'category': meta['category'],
                    'avg': result.get('avg', ''),
                    'std': result.get('std', ''),
                    'unit': result.get('unit', ''),
                    'batch': result.get('batch', ''),
                    'runs': result.get('runs', ''),
                    'sample_count': len(result.get('samples', [])),
                    'sample_values': ','.join(format_value(value) for value in result.get('samples', [])),
                })
    return rows


def build_comparison_rows(baseline_data, testing_data):
    baseline = baseline_data['results']
    testing = testing_data['results']
    rows = []
    for test_name in MEMORY_COMPARISON_TESTS:
        meta = TEST_META[test_name]
        base = entry(baseline, test_name)
        test = entry(testing, test_name)
        delta = improvement(baseline, testing, test_name, bigger_is_better=(meta['objective'] == 'higher'))
        relation = interval_relation(base, test)
        rows.append({
            'test': test_name,
            'label': meta['label'],
            'category': meta['category'],
            'objective': meta['objective'],
            'baseline_avg': metric_value(baseline, test_name),
            'baseline_error_bar': error_bar(baseline, test_name),
            'baseline_rounds': base.get('round_count', 0),
            'testing_avg': metric_value(testing, test_name),
            'testing_error_bar': error_bar(testing, test_name),
            'testing_rounds': test.get('round_count', 0),
            'unit': test.get('unit') or base.get('unit', ''),
            'improvement_percent': '' if delta is None else round(delta, 2),
            'interval_relation': relation,
            'comment': comparison_comment(delta, relation),
        })
    return rows


def build_scheduler_rows(testing_data):
    analysis = compute_scheduler_analysis(testing_data['results'])
    rows = []
    for scenario, item in analysis.items():
        for row in item['rows']:
            rows.append({
                'scenario': scenario,
                'algorithm': row['algorithm'],
                'avg': row['avg'],
                'error_bar': row['error_bar'],
                'std': row['std'],
                'unit': row['unit'],
                'round_count': row['round_count'],
                'winner': 'yes' if row['algorithm'] == item['winner'] else 'no',
                'scenario_spread_percent': round(item['spread_percent'], 2),
            })
    return rows


def write_csv(path, rows):
    ensure_output_dirs()
    path = Path(path)
    if not rows:
        path.write_text('', encoding='utf-8')
        return
    with path.open('w', newline='', encoding='utf-8') as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def save_results_json(payload):
    ensure_output_dirs()
    with (DATA_DIR / 'results.json').open('w', encoding='utf-8') as handle:
        json.dump(payload, handle, indent=2, ensure_ascii=False)


def uses_split_boot(dataset):
    commands = dataset['collection_summary'].get('unique_commands', [])
    return any('perftest memory + perftest sched' in command for command in commands)


def collection_summary_lines(label, dataset):
    summary = dataset['collection_summary']
    lines = [
        f'- {label} runs discovered: {len(dataset["runs"])}',
        f'- {label} complete runs: {summary.get("complete_runs", 0)}',
        f'- {label} partial runs: {summary.get("partial_runs", 0)}',
        f'- {label} raw-only recovered runs: {summary.get("raw_only_runs", 0)}',
    ]
    if summary.get('unique_commits'):
        lines.append(f'- {label} commit(s): {", ".join(summary["unique_commits"])}')
    if summary.get('unique_commands'):
        lines.append(f'- {label} command(s): {", ".join(summary["unique_commands"])}')
    if label.lower() == 'testing' and summary.get('unique_commands'):
        mode = 'split memory/scheduler boots' if uses_split_boot(dataset) else 'single combined boot'
        lines.append(f'- {label} collection mode: {mode}')
    if summary.get('unique_cpus'):
        lines.append(f'- {label} CPU setting(s): {", ".join(summary["unique_cpus"])}')
    partial_runs = [run['run_id'] for run in dataset.get('run_statuses', []) if run.get('status') == 'partial']
    if partial_runs:
        lines.append(f'- {label} partial run IDs: {", ".join(partial_runs)}')
    if dataset.get('aggregation_errors'):
        lines.append(f'- {label} mixed-signature tests: {len(dataset["aggregation_errors"])}')
    return lines


def build_recommendations(payload):
    recommendations = []
    for branch_name in ('baseline', 'testing'):
        dataset = payload[branch_name]
        summary = dataset['collection_summary']
        complete_runs = summary.get('complete_runs', 0)
        if complete_runs < 3:
            recommendations.append(
                f'[{branch_name}] add at least {3 - complete_runs} more complete round(s) with `./experiments/analyze_all.sh collect --rounds 3 --resume` before using the results as formal thesis evidence.'
            )
        partial_runs = [run['run_id'] for run in dataset.get('run_statuses', []) if run.get('status') == 'partial']
        if partial_runs:
            recommendations.append(
                f'[{branch_name}] keep partial runs ({", ".join(partial_runs)}) as audit evidence only; do not count them as formal rounds.'
            )
        raw_only_runs = [run['run_id'] for run in dataset.get('run_statuses', []) if run.get('source_kind') == 'raw']
        if raw_only_runs:
            recommendations.append(
                f'[{branch_name}] rerun with `--resume` if you want clean, complete replacements for recovered raw-only runs ({", ".join(raw_only_runs)}).'
            )
        if branch_name == 'testing' and summary.get('unique_commands') and not uses_split_boot(dataset):
            recommendations.append(
                '[testing] rerun formal data collection with split boots (`perftest memory` + `perftest sched`) to keep scheduler evidence isolated from the memory suite.'
            )
        if dataset.get('aggregation_errors'):
            recommendations.append(
                f'[{branch_name}] recollect or split datasets before aggregation; {len(dataset["aggregation_errors"])} test(s) currently mix different unit/batch/runs signatures.'
            )

    for item in payload['quality']['low_resolution_tests']:
        recommendations.append(
            f'[{item["branch"]}] increase `{item["test"]}` batch beyond {item.get("batch", 1)} to move complete run {item.get("run_id", "unknown")} out of the 1-3 tick range.'
        )

    deduped = []
    seen = set()
    for item in recommendations:
        if item not in seen:
            deduped.append(item)
            seen.add(item)
    return deduped


def save_markdown_report(payload):
    ensure_output_dirs()
    quality = payload['quality']
    scheduler = payload['scheduler_analysis']
    lines = [
        '# Module Three Experiment Report',
        '',
        f'- Generated at: {payload["generated_at"]}',
        f'- Baseline runs detected: {quality["baseline_rounds_detected"]}',
        f'- Testing runs detected: {quality["testing_rounds_detected"]}',
        '- Error bars use cross-round 95% CI only when at least two valid rounds are available; otherwise plots show descriptive in-run spread and the comparison table is labeled `insufficient-rounds`.',
        '- Mixed unit/batch/runs signatures are treated as invalid datasets and must be recollected or split before inferential comparison.',
        '- Raw in-run sample series are retained in JSON/CSV for appendix tables or follow-up significance checks, while the primary charts still use cross-round summaries over boot-level repetitions.',
        f'- Data quality score: {quality["readiness_score"]}/100',
        '',
        '## Data Quality',
        '',
        f'- Coverage: {quality["coverage_percent"]:.1f}%',
        f'- Repeatability: {quality["repeatability_percent"]:.1f}%',
        f'- Scheduler discrimination score: {quality["scheduler_spread_score"]:.1f}',
        f'- Stability score: {quality["stability_score"]:.1f}',
        f'- Mixed-signature items: {len(quality["aggregation_errors"])}',
        f'- Low-resolution complete runs: {len(quality["low_resolution_tests"])}',
        f'- High-variation items: {len(quality["high_variation_tests"])}',
        '',
        '## Experiment Configuration',
        '',
    ]
    lines.extend(collection_summary_lines('Baseline', payload['baseline']))
    lines.append('')
    lines.extend(collection_summary_lines('Testing', payload['testing']))
    lines.extend([
        '',
        '## Memory Optimization Summary',
        '',
        '- Formal thesis memory conclusions in this version are limited to COW and Lazy Allocation.',
        '- Buddy and mmap results may still appear in raw JSON/logs for compatibility, but they are not part of the formal comparison table or thesis claims.',
        '',
        '| Test | Baseline | Testing | Delta | Interval | Interpretation |',
        '| --- | ---: | ---: | ---: | --- | --- |',
    ])
    for row in build_comparison_rows(payload['baseline'], payload['testing']):
        lines.append(
            f'| {row["label"]} | {format_value(row["baseline_avg"])} | {format_value(row["testing_avg"])} | {format_pct(row["improvement_percent"] if row["improvement_percent"] != "" else None)} | {row["interval_relation"]} | {row["comment"]} |'
        )

    lines.extend([
        '',
        '## Scheduler Scenario Summary',
        '',
        '| Scenario | Winner | Spread | Note |',
        '| --- | --- | ---: | --- |',
    ])
    scenario_notes = {
        'throughput': 'lower completion time is better',
        'convoy': 'lower short-job completion time is better',
        'fairness': 'lower CPU-time spread is fairer',
        'response': 'lower interactive latency is better',
    }
    for scenario, item in scheduler.items():
        lines.append(f'| {scenario} | {item["winner"] or "N/A"} | {item["spread_percent"]:.1f}% | {scenario_notes.get(scenario, "")} |')

    if quality['counterexamples']:
        lines.extend(['', '## Counterexamples Worth Discussing', ''])
        for item in quality['counterexamples']:
            lines.append(f'- {item["label"]}: {item["delta_percent"]:.1f}% relative to baseline.')

    if quality['high_variation_tests']:
        lines.extend(['', '## High-Variation Items', ''])
        for item in quality['high_variation_tests']:
            lines.append(f'- {item["branch"]}:{item["test"]} -> CV={item["cv_percent"]:.2f}%')

    if quality['aggregation_errors']:
        lines.extend(['', '## Invalid Mixed-Signature Items', ''])
        for item in quality['aggregation_errors']:
            lines.append(f'- {item["branch"]}:{item["test"]} -> {item["message"]}')

    if quality['low_resolution_tests']:
        lines.extend(['', '## Low-Resolution Complete Runs', ''])
        for item in quality['low_resolution_tests']:
            lines.append(
                f'- {item["branch"]}:{item["test"]}:{item.get("run_id", "unknown")} -> avg={format_value(item.get("avg"))}, batch={item.get("batch", "N/A")}'
            )

    if payload.get('recommendations'):
        lines.extend(['', '## Recommended Next Actions', ''])
        for item in payload['recommendations']:
            lines.append(f'- {item}')

    lines.extend([
        '',
        '## Output Artifacts',
        '',
        '- `experiments/outputs/data/results.json`',
        '- `experiments/outputs/data/results_long.csv`',
        '- `experiments/outputs/data/round_results.csv`',
        '- `experiments/outputs/data/comparison_summary.csv`',
        '- `experiments/outputs/data/scheduler_summary.csv`',
        '- `experiments/outputs/reports/experiment_report.md`',
        '- `experiments/outputs/reports/thesis_memory_table.tex`',
        '- `experiments/outputs/figures/*.png` / `experiments/outputs/figures/*.pdf`',
        '',
    ])
    (REPORT_DIR / 'experiment_report.md').write_text('\n'.join(lines), encoding='utf-8')


def save_latex_table(payload):
    rows = build_comparison_rows(payload['baseline'], payload['testing'])
    lines = [
        '% Auto-generated by experiments/analyze/plot_results.py',
        '\\begin{tabular}{lccc}',
        '\\hline',
        'Scenario & Baseline & Testing & Delta \\\\',
        '\\hline',
    ]
    for row in rows:
        delta = format_pct(row['improvement_percent'] if row['improvement_percent'] != '' else None).replace('%', '\\%')
        lines.append(f'{row["label"]} & {format_value(row["baseline_avg"])} & {format_value(row["testing_avg"])} & {delta} \\\\')
    lines.extend(['\\hline', '\\end{tabular}', ''])
    (REPORT_DIR / 'thesis_memory_table.tex').write_text('\n'.join(lines), encoding='utf-8')


def main():
    baseline_data = load_branch_dataset('baseline')
    testing_data = load_branch_dataset('testing')
    baseline = baseline_data['results']
    testing = testing_data['results']
    if not baseline and not testing:
        print('[ERROR] No test results found')
        return 1

    setup_style()
    ensure_output_dirs()
    plot_cow_comparison(baseline, testing)
    plot_lazy_comparison(baseline, testing)
    plot_memory_overall(baseline, testing)
    plot_scheduler_comparison(testing)
    plot_scheduler_scenarios(testing)
    plot_overall_radar(baseline, testing)
    plot_memory_speedups(baseline, testing)
    plot_variation_profile(baseline, testing)

    payload = {
        'generated_at': datetime.now().isoformat(),
        'baseline': baseline_data,
        'testing': testing_data,
        'summary': compute_summary(baseline_data, testing_data),
        'quality': compute_quality_report(baseline_data, testing_data),
        'scheduler_analysis': compute_scheduler_analysis(testing),
        'artifacts': {
            'results_json': str(DATA_DIR / 'results.json'),
            'results_long_csv': str(DATA_DIR / 'results_long.csv'),
            'round_results_csv': str(DATA_DIR / 'round_results.csv'),
            'comparison_csv': str(DATA_DIR / 'comparison_summary.csv'),
            'scheduler_csv': str(DATA_DIR / 'scheduler_summary.csv'),
            'report_md': str(REPORT_DIR / 'experiment_report.md'),
            'latex_table': str(REPORT_DIR / 'thesis_memory_table.tex'),
            'figures_dir': str(OUTPUT_DIR),
        },
    }
    payload['recommendations'] = build_recommendations(payload)

    write_csv(DATA_DIR / 'results_long.csv', build_long_rows(baseline_data, testing_data))
    write_csv(DATA_DIR / 'round_results.csv', build_round_rows(baseline_data, testing_data))
    write_csv(DATA_DIR / 'comparison_summary.csv', build_comparison_rows(baseline_data, testing_data))
    write_csv(DATA_DIR / 'scheduler_summary.csv', build_scheduler_rows(testing_data))
    save_results_json(payload)
    save_markdown_report(payload)
    save_latex_table(payload)

    print('Chart generation complete.')
    print(f'Figures: {OUTPUT_DIR}/')
    print(f'Data: {DATA_DIR}/results.json')
    print(f'CSV: {DATA_DIR}/results_long.csv, {DATA_DIR}/round_results.csv, {DATA_DIR}/comparison_summary.csv, {DATA_DIR}/scheduler_summary.csv')
    print(f'Reports: {REPORT_DIR}/experiment_report.md, {REPORT_DIR}/thesis_memory_table.tex')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
