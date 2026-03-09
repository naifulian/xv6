#!/usr/bin/env python3
"""
parse_log.py - 解析 xv6 测试日志文件

功能：
1. 解析 test.log 文件
2. 提取测试结果、调度器信息、内存统计等
3. 输出 JSON 格式数据

使用方法：
    python parse_log.py [log_file] [output_file]
    python parse_log.py                    # 默认 test.log -> data/parsed.json
    python parse_log.py mytest.log         # 指定输入文件
    python parse_log.py test.log out.json  # 指定输入输出
"""

import re
import json
import sys
import os
from datetime import datetime
from typing import Dict, List, Any, Optional


class TestLogParser:
    """测试日志解析器"""
    
    def __init__(self):
        self.data = {
            'parse_time': datetime.now().isoformat(),
            'test_summary': {
                'total': 0,
                'passed': 0,
                'failed': 0,
                'pass_rate': 0.0
            },
            'test_results': [],
            'scheduler_tests': {},
            'memory_tests': {},
            'system_info': {}
        }
    
    def parse_file(self, log_file: str) -> Dict[str, Any]:
        """解析日志文件"""
        if not os.path.exists(log_file):
            print(f"Error: File not found: {log_file}")
            return self.data
        
        with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        self._parse_test_summary(content)
        self._parse_test_results(content)
        self._parse_scheduler_tests(content)
        self._parse_memory_tests(content)
        
        return self.data
    
    def _parse_test_summary(self, content: str):
        """解析测试摘要"""
        summary_pattern = r'===== Test Summary =====\s*Total:\s*(\d+)\s*Passed:\s*(\d+)\s*Failed:\s*(\d+)'
        match = re.search(summary_pattern, content)
        
        if match:
            total = int(match.group(1))
            passed = int(match.group(2))
            failed = int(match.group(3))
            
            self.data['test_summary'] = {
                'total': total,
                'passed': passed,
                'failed': failed,
                'pass_rate': round(passed / total * 100, 2) if total > 0 else 0.0
            }
    
    def _parse_test_results(self, content: str):
        """解析每个测试的结果"""
        test_pattern = r'===== Test: (\w+) =====\s*Description: ([^\n]+)\s*(.*?)Result: (PASSED|FAILED)'
        matches = re.findall(test_pattern, content, re.DOTALL)
        
        for match in matches:
            test_name = match[0]
            description = match[1].strip()
            details = match[2]
            result = match[3]
            
            test_info = {
                'name': test_name,
                'description': description,
                'result': result,
                'details': self._parse_test_details(details)
            }
            
            self.data['test_results'].append(test_info)
    
    def _parse_test_details(self, details: str) -> Dict[str, Any]:
        """解析测试详情"""
        info = {}
        
        ok_matches = re.findall(r'OK:\s*([^\n]+)', details)
        if ok_matches:
            info['ok_messages'] = [m.strip() for m in ok_matches]
        
        fail_matches = re.findall(r'FAIL:\s*([^\n]+)', details)
        if fail_matches:
            info['fail_messages'] = [m.strip() for m in fail_matches]
        
        info_matches = re.findall(r'INFO:\s*([^\n]+)', details)
        if info_matches:
            info['info_messages'] = [m.strip() for m in info_matches]
        
        return info
    
    def _parse_scheduler_tests(self, content: str):
        """解析调度器测试"""
        sched_tests = {
            'fcfs': self._extract_sched_test(content, 'sched_fcfs_order'),
            'priority': self._extract_sched_test(content, 'sched_priority'),
            'sml': self._extract_sched_test(content, 'sched_sml'),
            'lottery': self._extract_sched_test(content, 'sched_lottery'),
            'sjf': self._extract_sched_test(content, 'sched_sjf'),
            'srtf': self._extract_sched_test(content, 'sched_srtf'),
            'mlfq': self._extract_sched_test(content, 'sched_mlfq'),
            'cfs': self._extract_sched_test(content, 'sched_cfs'),
            'default': self._extract_sched_test(content, 'sched_default')
        }
        
        self.data['scheduler_tests'] = sched_tests
    
    def _extract_sched_test(self, content: str, test_prefix: str) -> Dict[str, Any]:
        """提取单个调度器测试结果"""
        pattern = rf'\[TEST\] {test_prefix}[^\n]*\n(.*?)(?=Result:|===== Test:)'
        match = re.search(pattern, content, re.DOTALL)
        
        result = {
            'tested': False,
            'passed': False,
            'switch_ok': False,
            'processes_created': 0
        }
        
        if match:
            test_content = match.group(1)
            result['tested'] = True
            
            if 'switched to' in test_content:
                result['switch_ok'] = True
            
            created_matches = re.findall(r'created process \d+', test_content)
            result['processes_created'] = len(created_matches)
            
            passed_pattern = rf'===== Test: {test_prefix}.*?Result: (PASSED|FAILED)'
            passed_match = re.search(passed_pattern, content, re.DOTALL)
            if passed_match:
                result['passed'] = passed_match.group(1) == 'PASSED'
        
        return result
    
    def _parse_memory_tests(self, content: str):
        """解析内存测试"""
        memory_info = {}
        
        buddy_pattern = r'\[TEST\] buddy.*?Result: (PASSED|FAILED)'
        buddy_matches = re.findall(buddy_pattern, content, re.DOTALL)
        memory_info['buddy_tests'] = {
            'total': len(buddy_matches),
            'passed': sum(1 for m in buddy_matches if m == 'PASSED')
        }
        
        cow_pattern = r'\[TEST\] cow.*?Result: (PASSED|FAILED)'
        cow_matches = re.findall(cow_pattern, content, re.DOTALL)
        memory_info['cow_tests'] = {
            'total': len(cow_matches),
            'passed': sum(1 for m in cow_matches if m == 'PASSED')
        }
        
        lazy_pattern = r'\[TEST\] lazy.*?Result: (PASSED|FAILED)'
        lazy_matches = re.findall(lazy_pattern, content, re.DOTALL)
        memory_info['lazy_tests'] = {
            'total': len(lazy_matches),
            'passed': sum(1 for m in lazy_matches if m == 'PASSED')
        }
        
        mmap_pattern = r'\[TEST\] mmap.*?Result: (PASSED|FAILED)'
        mmap_matches = re.findall(mmap_pattern, content, re.DOTALL)
        memory_info['mmap_tests'] = {
            'total': len(mmap_matches),
            'passed': sum(1 for m in mmap_matches if m == 'PASSED')
        }
        
        self.data['memory_tests'] = memory_info


def main():
    """主函数"""
    log_file = 'test.log'
    output_file = 'webui/data/parsed.json'
    
    if len(sys.argv) >= 2:
        log_file = sys.argv[1]
    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    
    print(f"Parsing log file: {log_file}")
    
    parser = TestLogParser()
    data = parser.parse_file(log_file)
    
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    
    print(f"Output saved to: {output_file}")
    print(f"\nTest Summary:")
    print(f"  Total:  {data['test_summary']['total']}")
    print(f"  Passed: {data['test_summary']['passed']}")
    print(f"  Failed: {data['test_summary']['failed']}")
    print(f"  Rate:   {data['test_summary']['pass_rate']}%")


if __name__ == '__main__':
    main()
