#!/bin/bash
# analyze_all.sh - 一键完成所有数据分析
# 使用方法: ./webui/analyze_all.sh

set -e

echo "========================================="
echo "xv6 数据分析一键脚本"
echo "========================================="
echo ""

# 检查数据文件 - 优先从 perfdata 目录复制
if [ -d "perfdata" ]; then
    echo "从 perfdata 目录复制数据..."
    mkdir -p webui/data
    cp perfdata/*.log webui/data/ 2>/dev/null || true
fi

# 检查数据文件是否存在
if [ ! -f "webui/data/baseline.log" ] && [ ! -f "perfdata/baseline.log" ]; then
    echo "错误: baseline 数据不存在"
    echo ""
    echo "请先收集数据:"
    echo "  git checkout baseline"
    echo "  ./collect_perfdata.sh"
    exit 1
fi

if [ ! -f "webui/data/testing.log" ] && [ ! -f "perfdata/testing.log" ]; then
    echo "错误: testing 数据不存在"
    echo ""
    echo "请先收集数据:"
    echo "  git checkout testing"
    echo "  ./collect_perfdata.sh"
    exit 1
fi

# 确保数据在 webui/data 目录
mkdir -p webui/data
cp perfdata/baseline.log webui/data/ 2>/dev/null || true
cp perfdata/testing.log webui/data/ 2>/dev/null || true

echo "数据文件检查通过"
echo ""

echo "[Step 1/3] 解析日志数据..."
python3 webui/parse_results.py

echo ""
echo "[Step 2/3] 生成图表..."
python3 webui/plot_figures.py

echo ""
echo "[Step 3/3] 生成报告..."
python3 webui/generate_report.py

echo ""
echo "========================================="
echo "分析完成!"
echo ""
echo "输出文件:"
echo "  - webui/data/results.json"
echo "  - webui/figures/*.png"
echo "  - webui/figures/*.pdf"
echo "  - webui/reports/experiment_report.md"
echo ""
echo "启动 Web 界面查看结果:"
echo "  cd webui && python3 -m http.server 8080"
echo "  然后访问 http://localhost:8080"
echo "========================================="
