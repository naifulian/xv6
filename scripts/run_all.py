#!/usr/bin/env python3
"""
run_all.py - 一键运行所有数据处理脚本

功能：
1. 解析日志文件
2. 分析数据
3. 生成 JSON 文件
4. 生成图表

使用方法：
    python run_all.py
    python run_all.py test.log  # 指定日志文件
"""

import os
import sys
import subprocess


def run_command(cmd: str, description: str) -> bool:
    """运行命令并返回是否成功"""
    print(f"\n{'='*60}")
    print(f"Step: {description}")
    print(f"{'='*60}")
    
    result = subprocess.run(cmd, shell=True)
    
    if result.returncode != 0:
        print(f"Error: {description} failed")
        return False
    
    return True


def main():
    """主函数"""
    log_file = sys.argv[1] if len(sys.argv) > 1 else 'test.log'
    
    print("="*60)
    print("xv6-riscv Data Processing Pipeline")
    print("="*60)
    print(f"Log file: {log_file}")
    print()
    
    steps = [
        (f"python3 scripts/parse_log.py {log_file}", "Parse log file"),
        ("python3 scripts/analyze_data.py", "Analyze data"),
        ("python3 scripts/generate_json.py", "Generate JSON files"),
        ("python3 scripts/generate_charts.py", "Generate charts"),
    ]
    
    for cmd, desc in steps:
        if not run_command(cmd, desc):
            print(f"\nPipeline stopped due to error in: {desc}")
            sys.exit(1)
    
    print("\n" + "="*60)
    print("Pipeline completed successfully!")
    print("="*60)
    print("\nGenerated files:")
    print("  - webui/data/parsed.json")
    print("  - webui/data/analysis.json")
    print("  - webui/data/report.txt")
    print("  - webui/data/scheduler.json")
    print("  - webui/data/memory.json")
    print("  - webui/data/performance.json")
    print("  - webui/data/overview.json")
    print("  - webui/charts/scheduler_comparison.png")
    print("  - webui/charts/memory_tests.png")
    print("  - webui/charts/test_summary.png")
    print("  - webui/charts/performance_radar.png")
    print("\nNext steps:")
    print("  cd webui && python -m http.server 8080")
    print("  Open http://localhost:8080 in browser")


if __name__ == '__main__':
    main()
