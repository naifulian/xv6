#!/usr/bin/env python3
"""
analyze_data.py - 数据分析脚本

功能：
1. 分析测试结果数据
2. 计算性能指标
3. 生成分析报告

使用方法：
    python analyze_data.py [input_file] [output_file]
    python analyze_data.py                           # 默认配置
    python analyze_data.py parsed.json analysis.json # 指定文件
"""

import json
import sys
import os
from datetime import datetime
from typing import Dict, List, Any


class DataAnalyzer:
    """数据分析器"""
    
    def __init__(self):
        self.analysis = {
            'analysis_time': datetime.now().isoformat(),
            'overview': {},
            'scheduler_analysis': {},
            'memory_analysis': {},
            'recommendations': []
        }
    
    def analyze(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """执行分析"""
        self._analyze_overview(data)
        self._analyze_scheduler(data)
        self._analyze_memory(data)
        self._generate_recommendations(data)
        
        return self.analysis
    
    def _analyze_overview(self, data: Dict[str, Any]):
        """分析总体概况"""
        summary = data.get('test_summary', {})
        
        self.analysis['overview'] = {
            'total_tests': summary.get('total', 0),
            'passed_tests': summary.get('passed', 0),
            'failed_tests': summary.get('failed', 0),
            'pass_rate': summary.get('pass_rate', 0.0),
            'status': 'excellent' if summary.get('pass_rate', 0) >= 95 else
                      'good' if summary.get('pass_rate', 0) >= 80 else
                      'needs_improvement'
        }
    
    def _analyze_scheduler(self, data: Dict[str, Any]):
        """分析调度器测试"""
        sched_tests = data.get('scheduler_tests', {})
        
        schedulers = ['default', 'fcfs', 'priority', 'sml', 'lottery', 
                      'sjf', 'srtf', 'mlfq', 'cfs']
        
        analysis = {
            'total_schedulers': len(schedulers),
            'tested_schedulers': 0,
            'passed_schedulers': 0,
            'details': {}
        }
        
        for sched in schedulers:
            test_info = sched_tests.get(sched, {})
            if test_info.get('tested', False):
                analysis['tested_schedulers'] += 1
                if test_info.get('passed', False):
                    analysis['passed_schedulers'] += 1
                
                analysis['details'][sched] = {
                    'tested': test_info.get('tested', False),
                    'passed': test_info.get('passed', False),
                    'switch_ok': test_info.get('switch_ok', False),
                    'processes_created': test_info.get('processes_created', 0)
                }
        
        analysis['coverage'] = round(
            analysis['tested_schedulers'] / analysis['total_schedulers'] * 100, 2
        ) if analysis['total_schedulers'] > 0 else 0
        
        self.analysis['scheduler_analysis'] = analysis
    
    def _analyze_memory(self, data: Dict[str, Any]):
        """分析内存测试"""
        mem_tests = data.get('memory_tests', {})
        
        analysis = {
            'buddy': self._analyze_test_category(mem_tests.get('buddy_tests', {})),
            'cow': self._analyze_test_category(mem_tests.get('cow_tests', {})),
            'lazy': self._analyze_test_category(mem_tests.get('lazy_tests', {})),
            'mmap': self._analyze_test_category(mem_tests.get('mmap_tests', {}))
        }
        
        total = sum(a['total'] for a in analysis.values())
        passed = sum(a['passed'] for a in analysis.values())
        
        analysis['overall'] = {
            'total': total,
            'passed': passed,
            'pass_rate': round(passed / total * 100, 2) if total > 0 else 0
        }
        
        self.analysis['memory_analysis'] = analysis
    
    def _analyze_test_category(self, test_data: Dict[str, Any]) -> Dict[str, Any]:
        """分析单个测试类别"""
        total = test_data.get('total', 0)
        passed = test_data.get('passed', 0)
        
        return {
            'total': total,
            'passed': passed,
            'failed': total - passed,
            'pass_rate': round(passed / total * 100, 2) if total > 0 else 0,
            'status': 'pass' if passed == total else
                      'partial' if passed > 0 else 'fail'
        }
    
    def _generate_recommendations(self, data: Dict[str, Any]):
        """生成建议"""
        recommendations = []
        
        summary = data.get('test_summary', {})
        if summary.get('failed', 0) > 0:
            recommendations.append({
                'type': 'critical',
                'message': f"有 {summary.get('failed', 0)} 个测试失败，建议优先修复"
            })
        
        sched_analysis = self.analysis.get('scheduler_analysis', {})
        if sched_analysis.get('coverage', 0) < 100:
            recommendations.append({
                'type': 'info',
                'message': f"调度器测试覆盖率 {sched_analysis.get('coverage', 0)}%，建议完善测试"
            })
        
        mem_analysis = self.analysis.get('memory_analysis', {})
        overall = mem_analysis.get('overall', {})
        if overall.get('pass_rate', 0) < 100:
            recommendations.append({
                'type': 'warning',
                'message': f"内存测试通过率 {overall.get('pass_rate', 0)}%，建议检查内存管理模块"
            })
        
        if not recommendations:
            recommendations.append({
                'type': 'success',
                'message': "所有测试通过，系统运行良好"
            })
        
        self.analysis['recommendations'] = recommendations


def generate_report(analysis: Dict[str, Any]) -> str:
    """生成文本报告"""
    lines = []
    
    lines.append("=" * 60)
    lines.append("xv6-riscv 测试分析报告")
    lines.append("=" * 60)
    lines.append(f"分析时间: {analysis.get('analysis_time', 'N/A')}")
    lines.append("")
    
    overview = analysis.get('overview', {})
    lines.append("## 总体概况")
    lines.append(f"  总测试数: {overview.get('total_tests', 0)}")
    lines.append(f"  通过数:   {overview.get('passed_tests', 0)}")
    lines.append(f"  失败数:   {overview.get('failed_tests', 0)}")
    lines.append(f"  通过率:   {overview.get('pass_rate', 0)}%")
    lines.append(f"  状态:     {overview.get('status', 'N/A')}")
    lines.append("")
    
    sched = analysis.get('scheduler_analysis', {})
    lines.append("## 调度器分析")
    lines.append(f"  测试覆盖率: {sched.get('coverage', 0)}%")
    lines.append(f"  已测试:     {sched.get('tested_schedulers', 0)}/{sched.get('total_schedulers', 0)}")
    lines.append(f"  通过数:     {sched.get('passed_schedulers', 0)}")
    lines.append("")
    
    mem = analysis.get('memory_analysis', {})
    lines.append("## 内存管理分析")
    lines.append(f"  Buddy System: {mem.get('buddy', {}).get('pass_rate', 0)}%")
    lines.append(f"  COW:          {mem.get('cow', {}).get('pass_rate', 0)}%")
    lines.append(f"  Lazy Alloc:   {mem.get('lazy', {}).get('pass_rate', 0)}%")
    lines.append(f"  mmap:         {mem.get('mmap', {}).get('pass_rate', 0)}%")
    lines.append("")
    
    lines.append("## 建议")
    for rec in analysis.get('recommendations', []):
        lines.append(f"  [{rec.get('type', 'info').upper()}] {rec.get('message', '')}")
    lines.append("")
    
    lines.append("=" * 60)
    
    return '\n'.join(lines)


def main():
    """主函数"""
    input_file = 'webui/data/parsed.json'
    output_file = 'webui/data/analysis.json'
    report_file = 'webui/data/report.txt'
    
    if len(sys.argv) >= 2:
        input_file = sys.argv[1]
    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    
    print(f"Analyzing data from: {input_file}")
    
    if not os.path.exists(input_file):
        print(f"Error: Input file not found: {input_file}")
        print("Please run parse_log.py first")
        return
    
    with open(input_file, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    analyzer = DataAnalyzer()
    analysis = analyzer.analyze(data)
    
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(analysis, f, indent=2, ensure_ascii=False)
    
    report = generate_report(analysis)
    with open(report_file, 'w', encoding='utf-8') as f:
        f.write(report)
    
    print(f"Analysis saved to: {output_file}")
    print(f"Report saved to: {report_file}")
    print("\n" + report)


if __name__ == '__main__':
    main()
