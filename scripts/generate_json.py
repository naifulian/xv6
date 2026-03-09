#!/usr/bin/env python3
"""
generate_json.py - 生成 Web 界面所需的 JSON 数据文件

功能：
1. 从解析数据生成 Web 界面所需的 JSON 文件
2. 生成 scheduler.json - 调度器对比数据
3. 生成 memory.json - 内存管理数据
4. 生成 performance.json - 性能指标数据

使用方法：
    python generate_json.py
"""

import json
import os
from datetime import datetime
from typing import Dict, Any, List


class JSONGenerator:
    """JSON 数据生成器"""
    
    def __init__(self, data_dir: str = 'webui/data'):
        self.data_dir = data_dir
        os.makedirs(data_dir, exist_ok=True)
    
    def generate_all(self, parsed_data: Dict[str, Any], analysis_data: Dict[str, Any]):
        """生成所有 JSON 文件"""
        self.generate_scheduler_json(parsed_data, analysis_data)
        self.generate_memory_json(parsed_data, analysis_data)
        self.generate_performance_json(parsed_data, analysis_data)
        self.generate_overview_json(parsed_data, analysis_data)
    
    def generate_scheduler_json(self, parsed_data: Dict[str, Any], analysis_data: Dict[str, Any]):
        """生成调度器对比数据"""
        sched_tests = parsed_data.get('scheduler_tests', {})
        sched_analysis = analysis_data.get('scheduler_analysis', {})
        
        schedulers = {
            'DEFAULT': {'id': 0, 'name': 'Round Robin', 'tested': False, 'passed': False},
            'FCFS': {'id': 1, 'name': 'First Come First Served', 'tested': False, 'passed': False},
            'PRIORITY': {'id': 2, 'name': 'Priority Scheduling', 'tested': False, 'passed': False},
            'SML': {'id': 3, 'name': 'Static Multilevel Queue', 'tested': False, 'passed': False},
            'LOTTERY': {'id': 4, 'name': 'Lottery Scheduling', 'tested': False, 'passed': False},
            'SJF': {'id': 5, 'name': 'Shortest Job First', 'tested': False, 'passed': False},
            'SRTF': {'id': 6, 'name': 'Shortest Remaining Time First', 'tested': False, 'passed': False},
            'MLFQ': {'id': 7, 'name': 'Multi-Level Feedback Queue', 'tested': False, 'passed': False},
            'CFS': {'id': 8, 'name': 'Completely Fair Scheduler', 'tested': False, 'passed': False}
        }
        
        sched_map = {
            'default': 'DEFAULT',
            'fcfs': 'FCFS',
            'priority': 'PRIORITY',
            'sml': 'SML',
            'lottery': 'LOTTERY',
            'sjf': 'SJF',
            'srtf': 'SRTF',
            'mlfq': 'MLFQ',
            'cfs': 'CFS'
        }
        
        for key, name in sched_map.items():
            test_info = sched_tests.get(key, {})
            schedulers[name]['tested'] = test_info.get('tested', False)
            schedulers[name]['passed'] = test_info.get('passed', False)
            schedulers[name]['switch_ok'] = test_info.get('switch_ok', False)
            schedulers[name]['processes_created'] = test_info.get('processes_created', 0)
        
        data = {
            'generated_at': datetime.now().isoformat(),
            'schedulers': schedulers,
            'summary': {
                'total': len(schedulers),
                'tested': sched_analysis.get('tested_schedulers', 0),
                'passed': sched_analysis.get('passed_schedulers', 0),
                'coverage': sched_analysis.get('coverage', 0)
            }
        }
        
        self._save_json('scheduler.json', data)
        print(f"Generated: scheduler.json")
    
    def generate_memory_json(self, parsed_data: Dict[str, Any], analysis_data: Dict[str, Any]):
        """生成内存管理数据"""
        mem_tests = parsed_data.get('memory_tests', {})
        mem_analysis = analysis_data.get('memory_analysis', {})
        
        data = {
            'generated_at': datetime.now().isoformat(),
            'categories': {
                'buddy': {
                    'name': 'Buddy System',
                    'description': '伙伴系统内存分配',
                    'total': mem_tests.get('buddy_tests', {}).get('total', 0),
                    'passed': mem_tests.get('buddy_tests', {}).get('passed', 0),
                    'pass_rate': mem_analysis.get('buddy', {}).get('pass_rate', 0),
                    'status': mem_analysis.get('buddy', {}).get('status', 'unknown')
                },
                'cow': {
                    'name': 'Copy-on-Write',
                    'description': '写时复制优化',
                    'total': mem_tests.get('cow_tests', {}).get('total', 0),
                    'passed': mem_tests.get('cow_tests', {}).get('passed', 0),
                    'pass_rate': mem_analysis.get('cow', {}).get('pass_rate', 0),
                    'status': mem_analysis.get('cow', {}).get('status', 'unknown')
                },
                'lazy': {
                    'name': 'Lazy Allocation',
                    'description': '懒分配优化',
                    'total': mem_tests.get('lazy_tests', {}).get('total', 0),
                    'passed': mem_tests.get('lazy_tests', {}).get('passed', 0),
                    'pass_rate': mem_analysis.get('lazy', {}).get('pass_rate', 0),
                    'status': mem_analysis.get('lazy', {}).get('status', 'unknown')
                },
                'mmap': {
                    'name': 'Memory Mapping',
                    'description': '内存映射系统调用',
                    'total': mem_tests.get('mmap_tests', {}).get('total', 0),
                    'passed': mem_tests.get('mmap_tests', {}).get('passed', 0),
                    'pass_rate': mem_analysis.get('mmap', {}).get('pass_rate', 0),
                    'status': mem_analysis.get('mmap', {}).get('status', 'unknown')
                }
            },
            'summary': {
                'total': mem_analysis.get('overall', {}).get('total', 0),
                'passed': mem_analysis.get('overall', {}).get('passed', 0),
                'pass_rate': mem_analysis.get('overall', {}).get('pass_rate', 0)
            }
        }
        
        self._save_json('memory.json', data)
        print(f"Generated: memory.json")
    
    def generate_performance_json(self, parsed_data: Dict[str, Any], analysis_data: Dict[str, Any]):
        """生成性能指标数据"""
        test_summary = parsed_data.get('test_summary', {})
        overview = analysis_data.get('overview', {})
        
        test_results = parsed_data.get('test_results', [])
        
        test_categories = {
            'scheduler': {'total': 0, 'passed': 0, 'time': 0},
            'memory': {'total': 0, 'passed': 0, 'time': 0},
            'integration': {'total': 0, 'passed': 0, 'time': 0},
            'boundary': {'total': 0, 'passed': 0, 'time': 0}
        }
        
        for test in test_results:
            name = test.get('name', '')
            result = test.get('result', '')
            
            if 'sched' in name:
                cat = 'scheduler'
            elif any(k in name for k in ['buddy', 'cow', 'lazy', 'mmap']):
                cat = 'memory'
            elif 'fork' in name or 'exec' in name:
                cat = 'integration'
            else:
                cat = 'boundary'
            
            test_categories[cat]['total'] += 1
            if result == 'PASSED':
                test_categories[cat]['passed'] += 1
        
        data = {
            'generated_at': datetime.now().isoformat(),
            'test_performance': {
                'total_tests': test_summary.get('total', 0),
                'passed_tests': test_summary.get('passed', 0),
                'failed_tests': test_summary.get('failed', 0),
                'pass_rate': test_summary.get('pass_rate', 0),
                'status': overview.get('status', 'unknown')
            },
            'categories': test_categories,
            'metrics': {
                'scheduler_coverage': analysis_data.get('scheduler_analysis', {}).get('coverage', 0),
                'memory_pass_rate': analysis_data.get('memory_analysis', {}).get('overall', {}).get('pass_rate', 0)
            }
        }
        
        self._save_json('performance.json', data)
        print(f"Generated: performance.json")
    
    def generate_overview_json(self, parsed_data: Dict[str, Any], analysis_data: Dict[str, Any]):
        """生成总览数据"""
        data = {
            'generated_at': datetime.now().isoformat(),
            'project': {
                'name': 'xv6-riscv 扩展项目',
                'version': '1.0.0',
                'modules': [
                    {'name': '调度算法优化', 'status': 'completed', 'tests': 18},
                    {'name': '内存管理优化', 'status': 'completed', 'tests': 34},
                    {'name': '软件工程测试', 'status': 'completed', 'tests': 68},
                    {'name': '数据收集分析', 'status': 'completed', 'tests': 0},
                    {'name': 'Web展示界面', 'status': 'in_progress', 'tests': 0}
                ]
            },
            'test_summary': parsed_data.get('test_summary', {}),
            'scheduler_summary': analysis_data.get('scheduler_analysis', {}),
            'memory_summary': analysis_data.get('memory_analysis', {}).get('overall', {}),
            'recommendations': analysis_data.get('recommendations', [])
        }
        
        self._save_json('overview.json', data)
        print(f"Generated: overview.json")
    
    def _save_json(self, filename: str, data: Dict[str, Any]):
        """保存 JSON 文件"""
        filepath = os.path.join(self.data_dir, filename)
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)


def main():
    """主函数"""
    print("Generating JSON files for Web UI...")
    print()
    
    parsed_file = 'webui/data/parsed.json'
    analysis_file = 'webui/data/analysis.json'
    
    if not os.path.exists(parsed_file):
        print(f"Error: {parsed_file} not found")
        print("Please run parse_log.py first")
        return
    
    if not os.path.exists(analysis_file):
        print(f"Error: {analysis_file} not found")
        print("Please run analyze_data.py first")
        return
    
    with open(parsed_file, 'r', encoding='utf-8') as f:
        parsed_data = json.load(f)
    
    with open(analysis_file, 'r', encoding='utf-8') as f:
        analysis_data = json.load(f)
    
    generator = JSONGenerator()
    generator.generate_all(parsed_data, analysis_data)
    
    print()
    print("All JSON files generated successfully!")
    print(f"Output directory: webui/data/")


if __name__ == '__main__':
    main()
