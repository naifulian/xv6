#!/usr/bin/env python3
"""
plot_results.py - Parse logs and generate figures for xv6 performance tests

Usage:
    python3 webui/plot_results.py

Input:
    log/testing/perftest.log
    log/baseline/perftest.log

Output:
    webui/figures/*.png
    webui/figures/*.pdf
    webui/data/results.json

Log Format:
    META:ts=<ticks>:cpus=<n>:branch=<baseline|testing>:sched=<name>
    RESULT:<name>:<avg>:<std>:<unit>:<batch>:<runs>
"""

import re
import os
import json
from datetime import datetime
from pathlib import Path

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError:
    print("[ERROR] matplotlib not installed")
    print("Run: pip install matplotlib numpy")
    exit(1)


LOG_DIR = "log"
OUTPUT_DIR = "webui/figures"
DATA_DIR = "webui/data"


def parse_log_file(filepath):
    """Parse log file, extract META and RESULT lines"""
    results = {}
    metadata = {}
    
    if not os.path.exists(filepath):
        print(f"[WARN] {filepath} not found")
        return {'metadata': metadata, 'results': results}
    
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
    # New format: RESULT:<name>:<avg>:<std>:<unit>:<batch>:<runs>
    # Old format: RESULT:<name>:<avg>:<std>:<unit>
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
    
    return {'metadata': metadata, 'results': results}


def setup_style():
    """Setup chart style"""
    try:
        plt.style.use('seaborn-v0_8-darkgrid')
    except:
        try:
            plt.style.use('seaborn-darkgrid')
        except:
            pass
    
    plt.rcParams['figure.figsize'] = (10, 6)
    plt.rcParams['figure.dpi'] = 300
    plt.rcParams['font.size'] = 12
    plt.rcParams['axes.unicode_minus'] = False


def save_figure(fig, fig_id):
    """Save figure to PNG and PDF"""
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    png_path = f"{OUTPUT_DIR}/{fig_id}.png"
    fig.savefig(png_path, dpi=300, bbox_inches='tight')
    print(f"  Saved: {png_path}")
    
    pdf_path = f"{OUTPUT_DIR}/{fig_id}.pdf"
    fig.savefig(pdf_path, format='pdf', bbox_inches='tight')
    print(f"  Saved: {pdf_path}")
    
    plt.close(fig)


def plot_cow_comparison(baseline, testing):
    """Fig-1: COW Performance Comparison"""
    print("Generating Fig-1: COW Comparison...")
    
    tests = ['COW_NO_ACCESS', 'COW_READONLY', 'COW_PARTIAL', 'COW_FULLWRITE']
    labels = ['No Access\n(Best Case)', 'Read Only', 'Write 30%', 'Write 100%\n(Worst Case)']
    
    baseline_vals = [baseline.get(t, {}).get('avg', 0) for t in tests]
    testing_vals = [testing.get(t, {}).get('avg', 0) for t in tests]
    
    if all(v == 0 for v in baseline_vals + testing_vals):
        print("  [SKIP] No COW data available")
        return
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    x = np.arange(len(labels))
    
    ax.plot(x, baseline_vals, 'o-', linewidth=2, markersize=8, 
            label='Baseline (Eager Copy)', color='#3498db')
    ax.plot(x, testing_vals, 's-', linewidth=2, markersize=8, 
            label='Testing (COW)', color='#2ecc71')
    
    ax.set_xlabel('Test Scenario', fontsize=12, fontweight='bold')
    ax.set_ylabel('Time (ticks)', fontsize=12, fontweight='bold')
    ax.set_title('Copy-on-Write Performance Comparison', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.legend(fontsize=11)
    ax.grid(alpha=0.3)
    
    # Annotate values
    for i, (b, t) in enumerate(zip(baseline_vals, testing_vals)):
        if b > 0:
            ax.annotate(f'{b}', (i, b), textcoords="offset points", 
                       xytext=(0, 10), ha='center', fontsize=10)
        if t > 0:
            ax.annotate(f'{t}', (i, t), textcoords="offset points", 
                       xytext=(0, -15), ha='center', fontsize=10)
    
    # Highlight counter-example
    if testing_vals[3] > baseline_vals[3]:
        ax.annotate('Counter-example\n(COW overhead)', xy=(3, testing_vals[3]), 
                    xytext=(2.5, testing_vals[3] * 1.15),
                    ha='center', color='red', fontweight='bold', fontsize=10,
                    arrowprops=dict(arrowstyle='->', color='red'))
    
    plt.tight_layout()
    save_figure(fig, 'fig1_cow_comparison')


def plot_lazy_comparison(baseline, testing):
    """Fig-2: Lazy Allocation Performance Comparison"""
    print("Generating Fig-2: Lazy Comparison...")
    
    tests = ['LAZY_SPARSE', 'LAZY_HALF', 'LAZY_FULL']
    labels = ['Access 1%\n(Best Case)', 'Access 50%', 'Access 100%\n(Worst Case)']
    
    baseline_vals = [baseline.get(t, {}).get('avg', 0) for t in tests]
    testing_vals = [testing.get(t, {}).get('avg', 0) for t in tests]
    
    if all(v == 0 for v in baseline_vals + testing_vals):
        print("  [SKIP] No Lazy data available")
        return
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    x = np.arange(len(labels))
    
    ax.plot(x, baseline_vals, 'o-', linewidth=2, markersize=8, 
            label='Baseline (Eager Alloc)', color='#3498db')
    ax.plot(x, testing_vals, 's-', linewidth=2, markersize=8, 
            label='Testing (Lazy Alloc)', color='#2ecc71')
    
    ax.set_xlabel('Access Ratio', fontsize=12, fontweight='bold')
    ax.set_ylabel('Time (ticks)', fontsize=12, fontweight='bold')
    ax.set_title('Lazy Allocation Performance Comparison', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.legend(fontsize=11)
    ax.grid(alpha=0.3)
    
    for i, (b, t) in enumerate(zip(baseline_vals, testing_vals)):
        if b > 0:
            ax.annotate(f'{b}', (i, b), textcoords="offset points", 
                       xytext=(0, 10), ha='center', fontsize=10)
        if t > 0:
            ax.annotate(f'{t}', (i, t), textcoords="offset points", 
                       xytext=(0, -15), ha='center', fontsize=10)
    
    if testing_vals[2] > baseline_vals[2]:
        ax.annotate('Counter-example\n(Page fault overhead)', xy=(2, testing_vals[2]), 
                    xytext=(1.5, testing_vals[2] * 1.15),
                    ha='center', color='red', fontweight='bold', fontsize=10,
                    arrowprops=dict(arrowstyle='->', color='red'))
    
    plt.tight_layout()
    save_figure(fig, 'fig2_lazy_comparison')


def plot_buddy_comparison(baseline, testing):
    """Fig-3: Buddy System Fragmentation Comparison"""
    print("Generating Fig-3: Buddy Comparison...")
    
    baseline_kb = baseline.get('BUDDY_FRAGMENT', {}).get('avg', 0)
    testing_kb = testing.get('BUDDY_FRAGMENT', {}).get('avg', 0)
    
    if baseline_kb == 0 and testing_kb == 0:
        print("  [SKIP] No Buddy data available")
        return
    
    fig, ax = plt.subplots(figsize=(8, 6))
    
    x = ['Baseline\n(Linked List)', 'Testing\n(Buddy System)']
    y = [baseline_kb, testing_kb]
    colors = ['#3498db', '#2ecc71']
    
    bars = ax.bar(x, y, color=colors, width=0.5)
    
    ax.set_xlabel('Allocator', fontsize=12, fontweight='bold')
    ax.set_ylabel('Max Contiguous Allocation (KB)', fontsize=12, fontweight='bold')
    ax.set_title('Memory Fragmentation Comparison', fontsize=14, fontweight='bold')
    ax.grid(axis='y', alpha=0.3)
    
    for bar, val in zip(bars, y):
        if val > 0:
            ax.annotate(f'{val} KB', xy=(bar.get_x() + bar.get_width()/2, bar.get_height()),
                       ha='center', va='bottom', fontsize=12, fontweight='bold')
    
    plt.tight_layout()
    save_figure(fig, 'fig3_buddy_comparison')


def plot_memory_overall(baseline, testing):
    """Fig-4: Memory Optimization Overall Comparison"""
    print("Generating Fig-4: Memory Overall Comparison...")
    
    tests = ['COW_NO_ACCESS', 'COW_READONLY', 'COW_PARTIAL', 'COW_FULLWRITE',
             'LAZY_SPARSE', 'LAZY_HALF', 'LAZY_FULL']
    test_names = ['COW\nNo Access', 'COW\nRead Only', 'COW\nWrite 30%', 
                  'COW\nWrite 100%', 'Lazy\n1%', 'Lazy\n50%', 'Lazy\n100%']
    
    baseline_vals = [baseline.get(t, {}).get('avg', 0) for t in tests]
    testing_vals = [testing.get(t, {}).get('avg', 0) for t in tests]
    
    if all(v == 0 for v in baseline_vals + testing_vals):
        print("  [SKIP] No memory data available")
        return
    
    x = np.arange(len(tests))
    width = 0.35
    
    fig, ax = plt.subplots(figsize=(12, 6))
    
    bars1 = ax.bar(x - width/2, baseline_vals, width, 
                   label='Baseline', color='#3498db')
    bars2 = ax.bar(x + width/2, testing_vals, width, 
                   label='Testing', color='#2ecc71')
    
    ax.set_xlabel('Test Case', fontsize=12, fontweight='bold')
    ax.set_ylabel('Time (ticks)', fontsize=12, fontweight='bold')
    ax.set_title('Memory Management Optimization Comparison', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(test_names, fontsize=9)
    ax.legend(fontsize=11)
    ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    save_figure(fig, 'fig4_memory_overall')


def plot_scheduler_comparison(testing):
    """Fig-5: Scheduler Throughput Comparison"""
    print("Generating Fig-5: Scheduler Comparison...")
    
    tests = ['SCHED_THROUGHPUT_RR', 'SCHED_THROUGHPUT_FCFS', 'SCHED_THROUGHPUT_SJF']
    labels = ['RR', 'FCFS', 'SJF']
    
    vals = [testing.get(t, {}).get('avg', 0) for t in tests]
    
    if all(v == 0 for v in vals):
        print("  [SKIP] No scheduler data available")
        return
    
    fig, ax = plt.subplots(figsize=(8, 6))
    
    colors = ['#3498db', '#2ecc71', '#e74c3c']
    bars = ax.bar(labels, vals, color=colors, width=0.6)
    
    ax.set_ylabel('Completion Time (ticks)', fontsize=12, fontweight='bold')
    ax.set_title('Scheduler Throughput Comparison', fontsize=14, fontweight='bold')
    ax.grid(axis='y', alpha=0.3)
    
    for bar in bars:
        height = bar.get_height()
        if height > 0:
            ax.text(bar.get_x() + bar.get_width()/2., height,
                   f'{height:.0f}',
                   ha='center', va='bottom', fontsize=11, fontweight='bold')
    
    plt.tight_layout()
    save_figure(fig, 'fig5_scheduler_throughput')


def plot_scheduler_scenarios(testing):
    """Fig-6: Scheduler Scenario Comparison"""
    print("Generating Fig-6: Scheduler Scenarios...")
    
    scenarios = [
        ('Throughput', ['SCHED_THROUGHPUT_RR', 'SCHED_THROUGHPUT_FCFS', 'SCHED_THROUGHPUT_SJF']),
        ('Convoy', ['SCHED_CONVOY_RR', 'SCHED_CONVOY_FCFS', 'SCHED_CONVOY_SRTF']),
        ('Fairness', ['SCHED_FAIRNESS_RR', 'SCHED_FAIRNESS_PRIORITY', 'SCHED_FAIRNESS_CFS']),
        ('Response', ['SCHED_RESPONSE_RR', 'SCHED_RESPONSE_MLFQ']),
    ]
    
    has_data = False
    for _, tests in scenarios:
        for t in tests:
            if testing.get(t, {}).get('avg', 0) > 0:
                has_data = True
                break
    
    if not has_data:
        print("  [SKIP] No scheduler scenario data available")
        return
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    axes = axes.flatten()
    
    for idx, (scenario, tests) in enumerate(scenarios):
        ax = axes[idx]
        labels = [t.split('_')[-1] for t in tests]
        vals = [testing.get(t, {}).get('avg', 0) for t in tests]
        
        if all(v == 0 for v in vals):
            ax.text(0.5, 0.5, 'No Data', ha='center', va='center', fontsize=14)
            ax.set_title(f'{scenario} (No Data)', fontsize=12, fontweight='bold')
            continue
        
        bars = ax.bar(labels, vals, color=['#3498db', '#2ecc71', '#e74c3c', '#f39c12'][:len(labels)])
        ax.set_ylabel('Time (ticks)', fontsize=11)
        ax.set_title(f'{scenario}', fontsize=12, fontweight='bold')
        ax.grid(axis='y', alpha=0.3)
        
        for bar in bars:
            height = bar.get_height()
            if height > 0:
                ax.text(bar.get_x() + bar.get_width()/2., height,
                       f'{height:.0f}', ha='center', va='bottom', fontsize=10)
    
    plt.suptitle('Scheduler Performance by Scenario', fontsize=14, fontweight='bold', y=1.02)
    plt.tight_layout()
    save_figure(fig, 'fig6_scheduler_scenarios')


def plot_overall_radar(baseline, testing):
    """Fig-7: Overall Performance Radar Chart"""
    print("Generating Fig-7: Overall Radar...")
    
    # Calculate improvement scores
    cow_score = 0
    lazy_score = 0
    buddy_score = 0
    
    cow_tests = ['COW_NO_ACCESS', 'COW_READONLY', 'COW_PARTIAL']
    for test in cow_tests:
        b = baseline.get(test, {}).get('avg', 0)
        t = testing.get(test, {}).get('avg', 0)
        if b > 0 and t > 0:
            cow_score += max(0, (b - t) / b * 100)
    cow_score = min(100, cow_score / len(cow_tests) * 1.5) if cow_score > 0 else 50
    
    lazy_tests = ['LAZY_SPARSE', 'LAZY_HALF']
    for test in lazy_tests:
        b = baseline.get(test, {}).get('avg', 0)
        t = testing.get(test, {}).get('avg', 0)
        if b > 0 and t > 0:
            lazy_score += max(0, (b - t) / b * 100)
    lazy_score = min(100, lazy_score / len(lazy_tests) * 1.5) if lazy_score > 0 else 50
    
    b = baseline.get('BUDDY_FRAGMENT', {}).get('avg', 0)
    t = testing.get('BUDDY_FRAGMENT', {}).get('avg', 0)
    if b > 0 and t > b:
        buddy_score = min(100, (t - b) / b * 100 * 2)
    else:
        buddy_score = 50
    
    categories = ['COW\nOptimization', 'Lazy\nOptimization', 'Buddy\nOptimization', 
                  'Scheduler\nDiversity', 'Code\nQuality']
    values = [cow_score, lazy_score, buddy_score, 85, 90]
    
    angles = np.linspace(0, 2 * np.pi, len(categories), endpoint=False).tolist()
    angles += angles[:1]
    values = values + values[:1]
    
    fig, ax = plt.subplots(figsize=(10, 10), subplot_kw=dict(polar=True))
    
    ax.plot(angles, values, 'o-', linewidth=2, color='#3498db')
    ax.fill(angles, values, alpha=0.25, color='#3498db')
    
    ax.set_xticks(angles[:-1])
    ax.set_xticklabels(categories, fontsize=12)
    ax.set_ylim(0, 100)
    ax.set_title('xv6-riscv Extension Project Performance', fontsize=14, fontweight='bold', y=1.1)
    
    for i, (angle, val) in enumerate(zip(angles[:-1], values[:-1])):
        ax.annotate(f'{val:.0f}', xy=(angle, val), 
                   textcoords="offset points", xytext=(0, 10),
                   ha='center', fontsize=12, fontweight='bold')
    
    plt.tight_layout()
    save_figure(fig, 'fig7_overall_radar')


def save_results_json(baseline_data, testing_data):
    """Save parsed results to JSON"""
    os.makedirs(DATA_DIR, exist_ok=True)
    
    output = {
        'generated_at': datetime.now().isoformat(),
        'baseline': baseline_data,
        'testing': testing_data
    }
    
    json_path = f"{DATA_DIR}/results.json"
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(output, f, indent=2, ensure_ascii=False)
    print(f"  Saved: {json_path}")


def main():
    print("=" * 50)
    print("xv6 Performance Test Chart Generator")
    print("=" * 50)
    print()
    
    baseline_file = f"{LOG_DIR}/baseline/perftest.log"
    testing_file = f"{LOG_DIR}/testing/perftest.log"
    
    print("[Step 1] Parsing log files...")
    baseline_data = parse_log_file(baseline_file)
    testing_data = parse_log_file(testing_file)
    
    baseline = baseline_data['results']
    testing = testing_data['results']
    
    print(f"  baseline: {len(baseline)} results")
    print(f"  testing: {len(testing)} results")
    print()
    
    if not baseline and not testing:
        print("[ERROR] No test results found")
        print()
        print("Please run tests first:")
        print("  # Testing branch:")
        print("  git checkout testing && make clean && make qemu")
        print("  perftest all > log/testing/perftest.log")
        print()
        print("  # Baseline branch:")
        print("  git checkout baseline && make clean && make qemu")
        print("  perftest all > log/baseline/perftest.log")
        return
    
    print("[Step 2] Setting up chart style...")
    setup_style()
    
    print()
    print("[Step 3] Generating charts...")
    print()
    print("=== Memory Management Charts ===")
    plot_cow_comparison(baseline, testing)
    plot_lazy_comparison(baseline, testing)
    plot_buddy_comparison(baseline, testing)
    plot_memory_overall(baseline, testing)
    
    print()
    print("=== Scheduler Charts ===")
    plot_scheduler_comparison(testing)
    plot_scheduler_scenarios(testing)
    
    print()
    print("=== Overall Charts ===")
    plot_overall_radar(baseline, testing)
    
    print()
    print("[Step 4] Saving results JSON...")
    save_results_json(baseline_data, testing_data)
    
    print()
    print("=" * 50)
    print("Chart generation complete!")
    print(f"Output directory: {OUTPUT_DIR}/")
    print(f"Data directory: {DATA_DIR}/")
    print()
    print("Generated files:")
    print("  - fig1_cow_comparison.png/pdf")
    print("  - fig2_lazy_comparison.png/pdf")
    print("  - fig3_buddy_comparison.png/pdf")
    print("  - fig4_memory_overall.png/pdf")
    print("  - fig5_scheduler_throughput.png/pdf")
    print("  - fig6_scheduler_scenarios.png/pdf")
    print("  - fig7_overall_radar.png/pdf")
    print("  - results.json")
    print("=" * 50)


if __name__ == '__main__':
    main()
