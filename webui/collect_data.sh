#!/bin/bash
#
# collect_data.sh - Collect performance data from xv6
#
# Usage:
#   ./webui/collect_data.sh testing    # Collect testing branch data
#   ./webui/collect_data.sh baseline   # Collect baseline branch data
#   ./webui/collect_data.sh all        # Collect both branches
#

set -e

LOG_DIR="log"
TIMEOUT=900  # 15 minutes timeout for QEMU
QEMU_CPUS=1

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_prerequisites() {
    if ! command -v expect &> /dev/null; then
        log_error "expect is not installed. Install with: sudo apt install expect"
        exit 1
    fi
    
    if ! command -v riscv64-unknown-elf-gcc &> /dev/null; then
        log_error "RISC-V toolchain not found. Please install riscv64-unknown-elf-gcc"
        exit 1
    fi
}

collect_branch() {
    local branch=$1
    local output_dir="${LOG_DIR}/${branch}"
    
    log_info "Starting data collection for ${branch} branch"
    
    # Check if branch exists
    if ! git rev-parse --verify ${branch} &> /dev/null; then
        log_error "Branch ${branch} does not exist"
        return 1
    fi
    
    # Create output directory
    mkdir -p ${output_dir}
    
    # Save current branch
    local current_branch=$(git branch --show-current)
    
    # Switch to target branch
    log_info "Switching to ${branch} branch..."
    git checkout ${branch}
    
    # Build
    log_info "Building xv6 for a stable single-core experiment run..."
    make clean > /dev/null 2>&1 || true
    make CPUS=${QEMU_CPUS} > /dev/null 2>&1
    
    # Run perftest using expect
    log_info "Running perftest (this may take several minutes)..."
    
    local output_file="${output_dir}/perftest.log"
    
    # Create expect script
    cat > /tmp/xv6_perftest.exp << EOF
#!/usr/bin/expect -f
set timeout ${TIMEOUT}

spawn make CPUS=${QEMU_CPUS} qemu

# Wait for shell prompt
expect {
    "init: starting sh" { }
    timeout { 
        puts "ERROR: Timeout waiting for shell"
        exit 1
    }
}

expect "\$ "

# Run perftest
send "perftest all\r"

# Wait for completion
expect {
    "Tests Complete" { }
    timeout {
        puts "ERROR: Timeout running perftest"
        exit 1
    }
}

expect "\$ "

# Exit xv6
send "\001x"

expect eof
EOF

    chmod +x /tmp/xv6_perftest.exp
    
    # Run expect script
    local status=0
    if /tmp/xv6_perftest.exp > ${output_file} 2>&1; then
        log_info "Data collection completed: ${output_file}"
    else
        status=1
        log_error "Data collection failed"
        cat ${output_file}
    fi
    
    # Clean up
    rm -f /tmp/xv6_perftest.exp
    
    # Switch back to original branch
    log_info "Switching back to ${current_branch}..."
    git checkout ${current_branch}
    
    if [ ${status} -ne 0 ]; then
        return ${status}
    fi
    
    # Show summary
    echo ""
    log_info "Summary for ${branch}:"
    grep "RESULT:" ${output_file} | head -10
    echo "..."
    local result_count=$(grep -c "RESULT:" ${output_file})
    log_info "Total results: ${result_count}"
}

show_usage() {
    echo "Usage: $0 [testing|baseline|all]"
    echo ""
    echo "Commands:"
    echo "  testing   - Collect data from testing branch"
    echo "  baseline  - Collect data from baseline branch"
    echo "  all       - Collect data from both branches"
    echo ""
    echo "Examples:"
    echo "  $0 testing"
    echo "  $0 all"
}

main() {
    if [ $# -eq 0 ]; then
        show_usage
        exit 1
    fi
    
    check_prerequisites
    
    mkdir -p ${LOG_DIR}
    
    case "$1" in
        testing)
            collect_branch testing
            ;;
        baseline)
            collect_branch baseline
            ;;
        all)
            collect_branch testing
            echo ""
            collect_branch baseline
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            log_error "Unknown command: $1"
            show_usage
            exit 1
            ;;
    esac
    
    echo ""
    log_info "Data collection complete!"
    log_info "Run 'python3 webui/validate_logs.py' to validate the data"
    log_info "Run 'python3 webui/plot_results.py' to generate charts"
}

main "$@"
