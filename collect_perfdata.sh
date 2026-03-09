#!/bin/bash
# collect_perfdata.sh - 收集性能测试数据（不依赖 webui 目录）
# 使用方法: ./collect_perfdata.sh

CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
DATA_DIR="perfdata"

mkdir -p $DATA_DIR

if [ "$CURRENT_BRANCH" == "testing" ]; then
    LOG_FILE="$DATA_DIR/testing.log"
elif [ "$CURRENT_BRANCH" == "baseline" ]; then
    LOG_FILE="$DATA_DIR/baseline.log"
else
    echo "错误: 请切换到 testing 或 baseline 分支"
    exit 1
fi

echo "========================================="
echo "xv6 性能测试数据收集"
echo "========================================="
echo "当前分支: $CURRENT_BRANCH"
echo "日志文件: $LOG_FILE"
echo ""
echo "编译中..."
make clean > /dev/null 2>&1 || true
make > /dev/null 2>&1

echo ""
echo "启动 QEMU..."
echo ""
echo "请在 xv6 启动出现 $ 提示符后，输入 perftest 运行测试"
echo "测试完成后按 Ctrl+A 然后按 X 退出 QEMU"
echo ""
echo "========================================="

make qemu 2>&1 | tee $LOG_FILE

echo ""
echo "========================================="
echo "数据收集完成!"
echo "日志文件: $LOG_FILE"
RESULT_COUNT=$(grep -c "^RESULT:" $LOG_FILE 2>/dev/null || echo "0")
echo "测试结果: $RESULT_COUNT 个"

if [ "$RESULT_COUNT" -gt 0 ]; then
    echo ""
    echo "下一步:"
    if [ "$CURRENT_BRANCH" == "testing" ]; then
        echo "  git checkout baseline"
        echo "  ./collect_perfdata.sh"
    else
        echo "  git checkout testing"
        echo "  cp perfdata/baseline.log webui/data/"
        echo "  cp perfdata/testing.log webui/data/"
        echo "  ./webui/analyze_all.sh"
    fi
fi
echo "========================================="
