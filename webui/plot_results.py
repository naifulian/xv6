#!/usr/bin/env python3
"""Parse perftest logs and generate figures plus a web-friendly JSON summary."""

import json
import os
import re
from datetime import datetime

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError:
    print('[ERROR] matplotlib or numpy is not installed')
    raise SystemExit(1)

LOG_DIR = 'log'
OUTPUT_DIR = 'webui/figures'
DATA_DIR = 'webui/data'

COMMON_REQUIRED = [
    'SYS_TOTAL_PAGES', 'SYS_FREE_PAGES', 'COW_NO_ACCESS', 'COW_READONLY',
    'COW_PARTIAL', 'COW_FULLWRITE', 'LAZY_SPARSE', 'LAZY_HALF', 'LAZY_FULL',
    'BUDDY_FRAGMENT',
]
TESTING_REQUIRED = COMMON_REQUIRED + [
    'MMAP_SPARSE', 'MMAP_FULL', 'SCHED_THROUGHPUT_RR', 'SCHED_THROUGHPUT_FCFS',
    'SCHED_THROUGHPUT_SJF', 'SCHED_CONVOY_RR', 'SCHED_CONVOY_FCFS',
    'SCHED_CONVOY_SRTF', 'SCHED_FAIRNESS_RR', 'SCHED_FAIRNESS_PRIORITY',
    'SCHED_FAIRNESS_CFS', 'SCHED_RESPONSE_RR', 'SCHED_RESPONSE_MLFQ',
]
SCENARIOS = {
    'throughput': ['SCHED_THROUGHPUT_RR', 'SCHED_THROUGHPUT_FCFS', 'SCHED_THROUGHPUT_SJF'],
    'convoy': ['SCHED_CONVOY_RR', 'SCHED_CONVOY_FCFS', 'SCHED_CONVOY_SRTF'],
    'fairness': ['SCHED_FAIRNESS_RR', 'SCHED_FAIRNESS_PRIORITY', 'SCHED_FAIRNESS_CFS'],
    'response': ['SCHED_RESPONSE_RR', 'SCHED_RESPONSE_MLFQ'],
}


def parse_log_file(filepath):
    results = {}
    metadata = {}

    if not os.path.exists(filepath):
        print(f'[WARN] {filepath} not found')
        return {'metadata': metadata, 'results': results}

    content = open(filepath, 'r', encoding='utf-8', errors='ignore').read()

    meta_pattern = r'META:ts=(\d+):cpus=(\d+):branch=(\w+):sched=(\w+)'
    meta_matches = re.findall(meta_pattern, content)
    if meta_matches:
        ts, cpus, branch, sched = meta_matches[-1]
        metadata = {'timestamp': int(ts), 'cpus': int(cpus), 'branch': branch, 'sched': sched}

    result_pattern = r'RESULT:(\w+):(-?\d+):(\d+):(\w+)(?::(\d+):(\d+))?'
    for name, avg, std, unit, batch, runs in re.findall(result_pattern, content):
        results[name] = {
            'avg': int(avg),
            'std': int(std),
            'unit': unit,
            'batch': int(batch) if batch else 1,
            'runs': int(runs) if runs else 1,
        }

    return {'metadata': metadata, 'results': results}


def setup_style():
    try:
        plt.style.use('seaborn-v0_8-darkgrid')
    except Exception:
        pass
    plt.rcParams['figure.dpi'] = 300
    plt.rcParams['font.size'] = 11
    plt.rcParams['axes.unicode_minus'] = False


def save_figure(fig, fig_id):
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    fig.savefig(f'{OUTPUT_DIR}/{fig_id}.png', dpi=300, bbox_inches='tight')
    fig.savefig(f'{OUTPUT_DIR}/{fig_id}.pdf', format='pdf', bbox_inches='tight')
    plt.close(fig)


def entry(results, name):
    return results.get(name, {})


def is_valid(result):
    return bool(result) and result.get('avg', -1) >= 0 and result.get('unit') != 'error'


def avg(results, name):
    item = entry(results, name)
    return item.get('avg', 0) if is_valid(item) else 0


def std(results, name):
    item = entry(results, name)
    return item.get('std', 0) if is_valid(item) else 0


def improvement(baseline, testing, test_name, bigger_is_better=False):
    b = entry(baseline, test_name)
    t = entry(testing, test_name)
    if not is_valid(b) or not is_valid(t) or b['avg'] == 0:
        return None
    if bigger_is_better:
        return (t['avg'] - b['avg']) * 100.0 / b['avg']
    return (b['avg'] - t['avg']) * 100.0 / b['avg']


def mean_positive(values):
    values = [v for v in values if v is not None]
    if not values:
        return 0.0
    return sum(values) / len(values)


def plot_cow_comparison(baseline, testing):
    tests = ['COW_NO_ACCESS', 'COW_READONLY', 'COW_PARTIAL', 'COW_FULLWRITE']
    labels = ['No Access', 'Read Only', 'Write 30%', 'Write 100%']
    base_vals = [avg(baseline, t) for t in tests]
    test_vals = [avg(testing, t) for t in tests]
    base_stds = [std(baseline, t) for t in tests]
    test_stds = [std(testing, t) for t in tests]

    if not any(base_vals + test_vals):
        return

    fig, ax = plt.subplots(figsize=(10, 6))
    x = np.arange(len(labels))
    ax.errorbar(x, base_vals, yerr=base_stds, fmt='o-', linewidth=2, capsize=4, label='Baseline (Eager Copy)', color='#2563eb')
    ax.errorbar(x, test_vals, yerr=test_stds, fmt='s-', linewidth=2, capsize=4, label='Testing (COW)', color='#059669')
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_xlabel('Scenario', fontweight='bold')
    ax.set_ylabel('Time (ticks)', fontweight='bold')
    ax.set_title('Copy-on-Write Performance Comparison', fontweight='bold')
    ax.legend()
    if test_vals[-1] > base_vals[-1] > 0:
        ax.annotate('Counter-example', xy=(x[-1], test_vals[-1]), xytext=(2.4, test_vals[-1] * 1.15),
                    color='red', fontweight='bold', arrowprops=dict(arrowstyle='->', color='red'))
    save_figure(fig, 'fig1_cow_comparison')


def plot_lazy_comparison(baseline, testing):
    tests = ['LAZY_SPARSE', 'LAZY_HALF', 'LAZY_FULL']
    labels = ['Access 1%', 'Access 50%', 'Access 100%']
    base_vals = [avg(baseline, t) for t in tests]
    test_vals = [avg(testing, t) for t in tests]
    base_stds = [std(baseline, t) for t in tests]
    test_stds = [std(testing, t) for t in tests]

    if not any(base_vals + test_vals):
        return

    fig, ax = plt.subplots(figsize=(10, 6))
    x = np.arange(len(labels))
    ax.errorbar(x, base_vals, yerr=base_stds, fmt='o-', linewidth=2, capsize=4, label='Baseline (Eager Alloc)', color='#2563eb')
    ax.errorbar(x, test_vals, yerr=test_stds, fmt='s-', linewidth=2, capsize=4, label='Testing (Lazy Alloc)', color='#059669')
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_xlabel('Access Ratio', fontweight='bold')
    ax.set_ylabel('Time (ticks)', fontweight='bold')
    ax.set_title('Lazy Allocation Performance Comparison', fontweight='bold')
    ax.legend()
    if test_vals[-1] > base_vals[-1] > 0:
        ax.annotate('Counter-example', xy=(x[-1], test_vals[-1]), xytext=(1.3, test_vals[-1] * 1.15),
                    color='red', fontweight='bold', arrowprops=dict(arrowstyle='->', color='red'))
    save_figure(fig, 'fig2_lazy_comparison')


def plot_buddy_comparison(baseline, testing):
    base = avg(baseline, 'BUDDY_FRAGMENT')
    test = avg(testing, 'BUDDY_FRAGMENT')
    if not (base or test):
        return

    fig, ax = plt.subplots(figsize=(8, 6))
    bars = ax.bar(['Baseline', 'Buddy'], [base, test], color=['#2563eb', '#059669'], width=0.55)
    ax.set_ylabel('Max Contiguous Allocation (KB)', fontweight='bold')
    ax.set_title('Buddy Fragmentation Comparison', fontweight='bold')
    for bar, value in zip(bars, [base, test]):
        ax.text(bar.get_x() + bar.get_width() / 2, value, str(value), ha='center', va='bottom', fontweight='bold')
    save_figure(fig, 'fig3_buddy_comparison')


def plot_memory_overall(baseline, testing):
    tests = ['COW_NO_ACCESS', 'COW_READONLY', 'COW_PARTIAL', 'COW_FULLWRITE', 'LAZY_SPARSE', 'LAZY_HALF', 'LAZY_FULL']
    labels = ['COW-No Access', 'COW-Read', 'COW-30%', 'COW-100%', 'Lazy-1%', 'Lazy-50%', 'Lazy-100%']
    base_vals = [avg(baseline, t) for t in tests]
    test_vals = [avg(testing, t) for t in tests]
    base_stds = [std(baseline, t) for t in tests]
    test_stds = [std(testing, t) for t in tests]

    if not any(base_vals + test_vals):
        return

    x = np.arange(len(labels))
    width = 0.38
    fig, ax = plt.subplots(figsize=(13, 6))
    ax.bar(x - width / 2, base_vals, width, yerr=base_stds, capsize=4, label='Baseline', color='#2563eb')
    ax.bar(x + width / 2, test_vals, width, yerr=test_stds, capsize=4, label='Testing', color='#059669')
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=20, ha='right')
    ax.set_ylabel('Time (ticks)', fontweight='bold')
    ax.set_title('Memory Optimization Overview', fontweight='bold')
    ax.legend()
    save_figure(fig, 'fig4_memory_overall')


def plot_scheduler_comparison(testing):
    tests = ['SCHED_THROUGHPUT_RR', 'SCHED_THROUGHPUT_FCFS', 'SCHED_THROUGHPUT_SJF']
    labels = ['RR', 'FCFS', 'SJF']
    vals = [avg(testing, t) for t in tests]
    errs = [std(testing, t) for t in tests]
    if not any(vals):
        return

    fig, ax = plt.subplots(figsize=(8, 6))
    bars = ax.bar(labels, vals, yerr=errs, capsize=4, color=['#2563eb', '#0891b2', '#dc2626'])
    ax.set_ylabel('Completion Time (ticks)', fontweight='bold')
    ax.set_title('Scheduler Throughput Comparison', fontweight='bold')
    for bar, value in zip(bars, vals):
        ax.text(bar.get_x() + bar.get_width() / 2, value, str(value), ha='center', va='bottom', fontweight='bold')
    save_figure(fig, 'fig5_scheduler_throughput')


def plot_scheduler_scenarios(testing):
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    axes = axes.flatten()
    has_data = False

    for idx, (scenario, tests) in enumerate(SCENARIOS.items()):
        labels = [t.split('_')[-1] for t in tests]
        vals = [avg(testing, t) for t in tests]
        errs = [std(testing, t) for t in tests]
        ax = axes[idx]
        if any(vals):
            has_data = True
            ax.bar(labels, vals, yerr=errs, capsize=4, color=['#2563eb', '#059669', '#dc2626'][:len(labels)])
            ax.set_title(scenario.title(), fontweight='bold')
            ax.set_ylabel('Time (ticks)')
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
        vals = [avg(testing, t) for t in tests if avg(testing, t) > 0]
        if len(vals) >= 2 and max(vals) > 0:
            scores.append((max(vals) - min(vals)) * 100.0 / max(vals))
    return sum(scores) / len(scores) if scores else 0.0


def plot_overall_radar(baseline, testing):
    completeness = 0.0
    valid_baseline = sum(1 for name in COMMON_REQUIRED if is_valid(entry(baseline, name)))
    valid_testing = sum(1 for name in TESTING_REQUIRED if is_valid(entry(testing, name)))
    completeness = (valid_baseline + valid_testing) * 100.0 / (len(COMMON_REQUIRED) + len(TESTING_REQUIRED))

    categories = ['COW', 'Lazy', 'Buddy', 'Scheduler', 'Completeness']
    values = [
        min(100.0, max(0.0, mean_positive([improvement(baseline, testing, name) for name in ['COW_NO_ACCESS', 'COW_READONLY', 'COW_PARTIAL']]))),
        min(100.0, max(0.0, mean_positive([improvement(baseline, testing, name) for name in ['LAZY_SPARSE', 'LAZY_HALF']]))),
        min(100.0, max(0.0, improvement(baseline, testing, 'BUDDY_FRAGMENT', bigger_is_better=True) or 0.0)),
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


def compute_summary(baseline_data, testing_data):
    baseline = baseline_data['results']
    testing = testing_data['results']
    summary = {
        'baseline_valid': sum(1 for name in COMMON_REQUIRED if is_valid(entry(baseline, name))),
        'baseline_expected': len(COMMON_REQUIRED),
        'testing_valid': sum(1 for name in TESTING_REQUIRED if is_valid(entry(testing, name))),
        'testing_expected': len(TESTING_REQUIRED),
        'improvements': {
            'cow_best_case': improvement(baseline, testing, 'COW_NO_ACCESS'),
            'cow_readonly': improvement(baseline, testing, 'COW_READONLY'),
            'cow_partial': improvement(baseline, testing, 'COW_PARTIAL'),
            'cow_fullwrite': improvement(baseline, testing, 'COW_FULLWRITE'),
            'lazy_sparse': improvement(baseline, testing, 'LAZY_SPARSE'),
            'lazy_half': improvement(baseline, testing, 'LAZY_HALF'),
            'lazy_full': improvement(baseline, testing, 'LAZY_FULL'),
            'buddy_fragment': improvement(baseline, testing, 'BUDDY_FRAGMENT', bigger_is_better=True),
        },
        'scheduler_spread_score': compute_scheduler_spread(testing),
    }
    return summary


def save_results_json(baseline_data, testing_data):
    os.makedirs(DATA_DIR, exist_ok=True)
    payload = {
        'generated_at': datetime.now().isoformat(),
        'baseline': baseline_data,
        'testing': testing_data,
        'summary': compute_summary(baseline_data, testing_data),
    }
    with open(f'{DATA_DIR}/results.json', 'w', encoding='utf-8') as handle:
        json.dump(payload, handle, indent=2, ensure_ascii=False)


def main():
    baseline_data = parse_log_file(f'{LOG_DIR}/baseline/perftest.log')
    testing_data = parse_log_file(f'{LOG_DIR}/testing/perftest.log')
    baseline = baseline_data['results']
    testing = testing_data['results']

    if not baseline and not testing:
        print('[ERROR] No test results found')
        return 1

    setup_style()
    plot_cow_comparison(baseline, testing)
    plot_lazy_comparison(baseline, testing)
    plot_buddy_comparison(baseline, testing)
    plot_memory_overall(baseline, testing)
    plot_scheduler_comparison(testing)
    plot_scheduler_scenarios(testing)
    plot_overall_radar(baseline, testing)
    save_results_json(baseline_data, testing_data)

    print('Chart generation complete.')
    print(f'Figures: {OUTPUT_DIR}/')
    print(f'Data: {DATA_DIR}/results.json')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
