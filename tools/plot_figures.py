#!/usr/bin/env python3
import json
import yaml
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
from pathlib import Path

def setup_style(config):
    """设置图表样式"""
    try:
        plt.style.use(config['quality']['style'])
    except:
        plt.style.use('seaborn-v0_8')
    plt.rcParams['font.size'] = config['quality']['font_size']
    plt.rcParams['figure.figsize'] = config['quality']['figure_size']
    plt.rcParams['figure.dpi'] = config['quality']['dpi']

def get_test_name(test_id, config):
    """获取测试名称"""
    for test in config['tests']:
        if test['id'] == test_id:
            return test['name']
    return test_id

def save_figure(fig, fig_id):
    """保存图表（PNG + PDF）"""
    output_dir = Path('figures')
    output_dir.mkdir(exist_ok=True)
    
    png_path = output_dir / f"{fig_id}.png"
    fig.savefig(png_path, dpi=300, bbox_inches='tight')
    print(f"  Saved: {png_path}")
    
    pdf_path = output_dir / f"{fig_id}.pdf"
    fig.savefig(pdf_path, format='pdf', bbox_inches='tight')
    print(f"  Saved: {pdf_path}")
    
    plt.close(fig)

def plot_line_chart(data, config, fig_config):
    """绘制折线图"""
    test_ids = fig_config['data']
    
    baseline_vals = [data['results'][tid]['baseline']['avg'] for tid in test_ids]
    testing_vals = [data['results'][tid]['testing']['avg'] for tid in test_ids]
    
    x_labels = [get_test_name(tid, config) for tid in test_ids]
    x = np.arange(len(x_labels))
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    ax.plot(x, baseline_vals, 'o-', linewidth=2, markersize=8, 
            label=fig_config['legend'][0], color='#1f77b4')
    ax.plot(x, testing_vals, 's-', linewidth=2, markersize=8, 
            label=fig_config['legend'][1], color='#ff7f0e')
    
    ax.set_xlabel(fig_config['x_label'], fontsize=12, fontweight='bold')
    ax.set_ylabel(fig_config['y_label'], fontsize=12, fontweight='bold')
    ax.set_title(fig_config['title'], fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels, rotation=15, ha='right')
    ax.legend(fontsize=11, loc='best')
    ax.grid(True, alpha=0.3)
    
    for i, (b, t) in enumerate(zip(baseline_vals, testing_vals)):
        ax.annotate(f'{b}', (i, b), textcoords="offset points", 
                   xytext=(0,10), ha='center', fontsize=9)
        ax.annotate(f'{t}', (i, t), textcoords="offset points", 
                   xytext=(0,-15), ha='center', fontsize=9)
    
    plt.tight_layout()
    save_figure(fig, fig_config['id'])

def plot_bar_chart(data, config, fig_config):
    """绘制柱状图"""
    test_ids = fig_config['data']
    
    baseline_vals = [data['results'][tid]['baseline']['avg'] for tid in test_ids]
    testing_vals = [data['results'][tid]['testing']['avg'] for tid in test_ids]
    
    x_labels = [get_test_name(tid, config) for tid in test_ids]
    x = np.arange(len(x_labels))
    width = 0.35
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    bars1 = ax.bar(x - width/2, baseline_vals, width, 
                   label=fig_config['legend'][0], color='#1f77b4', alpha=0.8)
    bars2 = ax.bar(x + width/2, testing_vals, width, 
                   label=fig_config['legend'][1], color='#ff7f0e', alpha=0.8)
    
    ax.set_xlabel(fig_config['x_label'], fontsize=12, fontweight='bold')
    ax.set_ylabel(fig_config['y_label'], fontsize=12, fontweight='bold')
    ax.set_title(fig_config['title'], fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels, rotation=15, ha='right')
    ax.legend(fontsize=11, loc='best')
    ax.grid(True, alpha=0.3, axis='y')
    
    for bars in [bars1, bars2]:
        for bar in bars:
            height = bar.get_height()
            ax.annotate(f'{height}',
                       xy=(bar.get_x() + bar.get_width() / 2, height),
                       xytext=(0, 3),
                       textcoords="offset points",
                       ha='center', va='bottom', fontsize=9)
    
    plt.tight_layout()
    save_figure(fig, fig_config['id'])

def main():
    print("[Step 1] Loading configuration...")
    with open('tools/config.yaml') as f:
        config = yaml.safe_load(f)
    
    print("[Step 2] Loading results...")
    if not Path('data/results.json').exists():
        print("[ERROR] data/results.json not found")
        print("        Run 'python3 tools/parse_results.py' first")
        return
    
    with open('data/results.json') as f:
        data = json.load(f)
    
    print("[Step 3] Setting up plot style...")
    setup_style(config)
    
    print("[Step 4] Generating figures...")
    for fig_config in config['figures']:
        print(f"  Generating {fig_config['id']}...")
        
        if fig_config['type'] == 'line':
            plot_line_chart(data, config, fig_config)
        elif fig_config['type'] == 'bar':
            plot_bar_chart(data, config, fig_config)
    
    print("\n========================================")
    print("All figures generated successfully!")
    print(f"Output directory: figures/")
    print("========================================")

if __name__ == '__main__':
    main()
