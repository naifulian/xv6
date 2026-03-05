#!/usr/bin/env python3
import re
import json
import yaml
from pathlib import Path
from datetime import datetime

def parse_log(filename):
    """解析日志文件，提取RESULT行"""
    results = {}
    with open(filename) as f:
        for line in f:
            match = re.match(r'RESULT:(\w+):(\d+):(\d+):(\w+)', line)
            if match:
                test_id, avg, std, unit = match.groups()
                results[test_id] = {
                    'avg': int(avg),
                    'std': int(std),
                    'unit': unit
                }
    return results

def validate_results(results, config):
    """验证结果完整性"""
    expected_tests = [t['id'] for t in config['tests']]
    missing = []
    
    for test_id in expected_tests:
        if test_id not in results:
            missing.append(test_id)
    
    if missing:
        print(f"[ERROR] Missing tests: {missing}")
        return False
    
    for test_id, data in results.items():
        if data['std'] > data['avg'] * 0.5:
            print(f"[WARNING] {test_id}: std ({data['std']}) > 50% of avg ({data['avg']})")
    
    return True

def calculate_improvement(baseline, testing):
    """计算性能提升"""
    comparison = {}
    
    for test_id in baseline:
        if test_id not in testing:
            print(f"[WARNING] {test_id} not found in testing results")
            continue
        
        b_avg = baseline[test_id]['avg']
        t_avg = testing[test_id]['avg']
        
        improvement = (b_avg - t_avg) / b_avg * 100
        
        comparison[test_id] = {
            'baseline': baseline[test_id],
            'testing': testing[test_id],
            'improvement': round(improvement, 1),
            'assessment': assess_improvement(improvement)
        }
    
    return comparison

def assess_improvement(improvement):
    """评估改进效果"""
    if improvement > 50:
        return "显著提升"
    elif improvement > 20:
        return "明显提升"
    elif improvement > 0:
        return "小幅提升"
    elif improvement > -10:
        return "基本持平"
    else:
        return "性能下降"

def main():
    print("[Step 1] Loading configuration...")
    with open('tools/config.yaml') as f:
        config = yaml.safe_load(f)
    
    print("[Step 2] Parsing baseline.log...")
    if not Path('data/baseline.log').exists():
        print("[ERROR] data/baseline.log not found")
        print("        Run './tools/collect_data.sh baseline' first")
        return
    
    baseline = parse_log('data/baseline.log')
    print(f"  Found {len(baseline)} test results")
    
    print("[Step 3] Parsing testing.log...")
    if not Path('data/testing.log').exists():
        print("[ERROR] data/testing.log not found")
        print("        Run './tools/collect_data.sh testing' first")
        return
    
    testing = parse_log('data/testing.log')
    print(f"  Found {len(testing)} test results")
    
    print("[Step 4] Validating results...")
    if not validate_results(baseline, config):
        print("[ERROR] Baseline results incomplete")
        return
    
    if not validate_results(testing, config):
        print("[ERROR] Testing results incomplete")
        return
    
    print("[Step 5] Calculating improvement...")
    comparison = calculate_improvement(baseline, testing)
    
    output = {
        'metadata': {
            'baseline_branch': 'baseline',
            'testing_branch': 'testing',
            'timestamp': datetime.now().isoformat()
        },
        'results': comparison
    }
    
    with open('data/results.json', 'w') as f:
        json.dump(output, f, indent=2)
    
    print(f"[Step 6] Results saved to data/results.json")
    print("\nSummary:")
    print(f"  Total tests: {len(comparison)}")
    print(f"  Improved: {sum(1 for v in comparison.values() if v['improvement'] > 0)}")
    print(f"  Degraded: {sum(1 for v in comparison.values() if v['improvement'] < 0)}")

if __name__ == '__main__':
    main()
