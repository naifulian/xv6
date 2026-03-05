#!/bin/bash
set -e

CONFIG_FILE="tools/config.yaml"
DATA_DIR="data"

echo "========================================="
echo "xv6 Data Collection Tool"
echo "========================================="

mkdir -p $DATA_DIR

collect_branch() {
    local branch=$1
    local log_file=$2
    
    echo ""
    echo "[Step 1] Switching to $branch branch..."
    CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
    if [ "$CURRENT_BRANCH" != "$branch" ]; then
        git checkout $branch
    else
        echo "  Already on $branch branch"
    fi
    
    echo "[Step 2] Building xv6..."
    make clean > /dev/null 2>&1
    make > /dev/null 2>&1
    
    echo "[Step 3] Running perftest (this may take a while)..."
    echo "         Saving output to $log_file"
    
    if command -v expect &> /dev/null; then
        expect << EOF > $log_file 2>&1
set timeout 300
spawn make qemu-nox
expect "$ "
send "perftest\r"
expect "Tests Complete"
send "\x01"
send "x"
expect eof
EOF
    else
        echo "[WARNING] expect not found, please run manually:"
        echo "  1. make qemu-nox"
        echo "  2. perftest"
        echo "  3. Copy output to $log_file"
        exit 1
    fi
    
    echo "[Step 4] Data collection complete for $branch"
    echo "         Found $(grep -c "^RESULT:" $log_file) test results"
}

if [ "$1" == "baseline" ]; then
    collect_branch "baseline" "$DATA_DIR/baseline.log"
    
elif [ "$1" == "testing" ]; then
    collect_branch "testing" "$DATA_DIR/testing.log"
    
elif [ "$1" == "all" ]; then
    collect_branch "baseline" "$DATA_DIR/baseline.log"
    collect_branch "testing" "$DATA_DIR/testing.log"
    
else
    echo "Usage: $0 [baseline|testing|all]"
    echo ""
    echo "Examples:"
    echo "  $0 baseline   # Collect data from baseline branch"
    echo "  $0 testing    # Collect data from testing branch"
    echo "  $0 all        # Collect data from both branches"
    exit 1
fi

echo ""
echo "========================================="
echo "Data collection complete!"
echo "Logs saved to: $DATA_DIR/"
echo "========================================="
