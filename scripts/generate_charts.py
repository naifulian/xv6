#!/usr/bin/env python3
"""
generate_charts.py - 生成图表

功能：
1. 使用 matplotlib 生成图表
2. 生成调度器对比图
3. 生成内存测试通过率图
4. 生成性能指标图

使用方法：
    python generate_charts.py

依赖：
    pip install matplotlib numpy
"""

import json
import os
import sys
from datetime import datetime
from typing import Dict, Any, List, Optional

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not installed. Charts will not be generated.")
    print("Install with: pip install matplotlib numpy")


class ChartGenerator:
    """图表生成器"""
    
    def __init__(self, data_dir: str = 'webui/data', output_dir: str = 'webui/charts'):
        self.data_dir = data_dir
        self.output_dir = output_dir
        os.makedirs(output_dir, exist_ok=True)
        
        self.colors = {
            'primary': '#667eea',
            'secondary': '#764ba2',
            'success': '#48bb78',
            'warning': '#ed8936',
            'danger': '#f56565',
            'info': '#4299e1'
        }
        
        plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial', 'Helvetica']
        plt.rcParams['axes.unicode_minus'] = False
        plt.rcParams['figure.figsize'] = (10, 6)
        plt.rcParams['figure.dpi'] = 100
    
    def generate_all(self):
        """生成所有图表"""
        if not HAS_MATPLOTLIB:
            print("Skipping chart generation - matplotlib not available")
            return
        
        self.generate_scheduler_chart()
        self.generate_memory_chart()
        self.generate_test_summary_chart()
        self.generate_performance_chart()
        
        print("All charts generated successfully!")
    
    def generate_scheduler_chart(self):
        """生成调度器测试对比图"""
        filepath = os.path.join(self.data_dir, 'scheduler.json')
        if not os.path.exists(filepath):
            print(f"Warning: {filepath} not found, skipping scheduler chart")
            return
        
        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        schedulers = data.get('schedulers', {})
        
        names = []
        tested = []
        passed = []
        
        for name, info in schedulers.items():
            names.append(name)
            tested.append(1 if info.get('tested', False) else 0)
            passed.append(1 if info.get('passed', False) else 0)
        
        x = np.arange(len(names))
        width = 0.35
        
        fig, ax = plt.subplots(figsize=(12, 6))
        
        bars1 = ax.bar(x - width/2, tested, width, label='Tested', color=self.colors['info'])
        bars2 = ax.bar(x + width/2, passed, width, label='Passed', color=self.colors['success'])
        
        ax.set_xlabel('Scheduler')
        ax.set_ylabel('Status (0/1)')
        ax.set_title('Scheduler Test Results')
        ax.set_xticks(x)
        ax.set_xticklabels(names, rotation=45, ha='right')
        ax.legend()
        ax.set_ylim(0, 1.5)
        
        ax.bar_label(bars1, labels=['Yes' if v else 'No' for v in tested], padding=3)
        ax.bar_label(bars2, labels=['Yes' if v else 'No' for v in passed], padding=3)
        
        plt.tight_layout()
        
        output_path = os.path.join(self.output_dir, 'scheduler_comparison.png')
        plt.savefig(output_path, bbox_inches='tight')
        plt.close()
        
        print(f"Generated: scheduler_comparison.png")
    
    def generate_memory_chart(self):
        """生成内存测试通过率图"""
        filepath = os.path.join(self.data_dir, 'memory.json')
        if not os.path.exists(filepath):
            print(f"Warning: {filepath} not found, skipping memory chart")
            return
        
        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        categories = data.get('categories', {})
        
        names = []
        pass_rates = []
        colors = []
        
        for key, info in categories.items():
            names.append(info.get('name', key))
            rate = info.get('pass_rate', 0)
            pass_rates.append(rate)
            
            if rate == 100:
                colors.append(self.colors['success'])
            elif rate >= 80:
                colors.append(self.colors['warning'])
            else:
                colors.append(self.colors['danger'])
        
        fig, ax = plt.subplots(figsize=(10, 6))
        
        bars = ax.bar(names, pass_rates, color=colors)
        
        ax.set_xlabel('Memory Category')
        ax.set_ylabel('Pass Rate (%)')
        ax.set_title('Memory Test Pass Rates')
        ax.set_ylim(0, 110)
        
        ax.bar_label(bars, fmt='%.1f%%', padding=3)
        
        ax.axhline(y=100, color='gray', linestyle='--', alpha=0.5, label='100% Line')
        ax.legend()
        
        plt.xticks(rotation=45, ha='right')
        plt.tight_layout()
        
        output_path = os.path.join(self.output_dir, 'memory_tests.png')
        plt.savefig(output_path, bbox_inches='tight')
        plt.close()
        
        print(f"Generated: memory_tests.png")
    
    def generate_test_summary_chart(self):
        """生成测试摘要饼图"""
        filepath = os.path.join(self.data_dir, 'performance.json')
        if not os.path.exists(filepath):
            print(f"Warning: {filepath} not found, skipping summary chart")
            return
        
        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        perf = data.get('test_performance', {})
        
        passed = perf.get('passed_tests', 0)
        failed = perf.get('failed_tests', 0)
        
        if passed + failed == 0:
            print("Warning: No test data available, skipping summary chart")
            return
        
        fig, ax = plt.subplots(figsize=(8, 8))
        
        sizes = [passed, failed]
        labels = [f'Passed ({passed})', f'Failed ({failed})']
        colors = [self.colors['success'], self.colors['danger']]
        explode = (0, 0.1)
        
        ax.pie(sizes, explode=explode, labels=labels, colors=colors,
               autopct='%1.1f%%', shadow=True, startangle=90)
        
        ax.set_title('Test Results Summary')
        
        plt.tight_layout()
        
        output_path = os.path.join(self.output_dir, 'test_summary.png')
        plt.savefig(output_path, bbox_inches='tight')
        plt.close()
        
        print(f"Generated: test_summary.png")
    
    def generate_performance_chart(self):
        """生成性能指标雷达图"""
        filepath = os.path.join(self.data_dir, 'performance.json')
        if not os.path.exists(filepath):
            print(f"Warning: {filepath} not found, skipping performance chart")
            return
        
        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        metrics = data.get('metrics', {})
        categories = data.get('categories', {})
        
        labels = ['Scheduler\nCoverage', 'Memory\nPass Rate', 
                  'Scheduler\nTests', 'Memory\nTests',
                  'Integration\nTests', 'Boundary\nTests']
        
        values = [
            metrics.get('scheduler_coverage', 0),
            metrics.get('memory_pass_rate', 0),
            self._calc_category_rate(categories.get('scheduler', {})),
            self._calc_category_rate(categories.get('memory', {})),
            self._calc_category_rate(categories.get('integration', {})),
            self._calc_category_rate(categories.get('boundary', {}))
        ]
        
        values += values[:1]
        
        angles = np.linspace(0, 2 * np.pi, len(labels), endpoint=False).tolist()
        angles += angles[:1]
        
        fig, ax = plt.subplots(figsize=(10, 10), subplot_kw=dict(polar=True))
        
        ax.plot(angles, values, 'o-', linewidth=2, color=self.colors['primary'])
        ax.fill(angles, values, alpha=0.25, color=self.colors['primary'])
        
        ax.set_xticks(angles[:-1])
        ax.set_xticklabels(labels)
        ax.set_ylim(0, 100)
        
        ax.set_title('Performance Metrics Radar', size=15, y=1.1)
        
        plt.tight_layout()
        
        output_path = os.path.join(self.output_dir, 'performance_radar.png')
        plt.savefig(output_path, bbox_inches='tight')
        plt.close()
        
        print(f"Generated: performance_radar.png")
    
    def _calc_category_rate(self, cat_data: Dict[str, Any]) -> float:
        """计算类别通过率"""
        total = cat_data.get('total', 0)
        passed = cat_data.get('passed', 0)
        return round(passed / total * 100, 2) if total > 0 else 0


def main():
    """主函数"""
    if not HAS_MATPLOTLIB:
        print("Error: matplotlib is required for chart generation")
        print("Install with: pip install matplotlib numpy")
        sys.exit(1)
    
    print("Generating charts...")
    print()
    
    generator = ChartGenerator()
    generator.generate_all()
    
    print()
    print(f"Output directory: webui/charts/")


if __name__ == '__main__':
    main()
