#!/bin/bash
# analyze_all.sh - Run collection, validation, and plotting for module three.

set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
MODE="all"

if [ $# -gt 0 ] && [[ "$1" != --* ]]; then
    MODE="$1"
    shift
fi

case "$MODE" in
    collect)
        "$ROOT/experiments/collect/collect_data.sh" all "$@"
        ;;
    validate)
        python3 "$ROOT/experiments/analyze/validate_logs.py"
        ;;
    plot)
        python3 "$ROOT/experiments/analyze/plot_results.py"
        ;;
    all)
        "$ROOT/experiments/collect/collect_data.sh" all "$@"
        python3 "$ROOT/experiments/analyze/validate_logs.py"
        python3 "$ROOT/experiments/analyze/plot_results.py"
        ;;
    -h|--help)
        cat <<EOF
Usage: $0 [all|collect|validate|plot] [collect options]

Examples:
  $0 all --rounds 5
  $0 collect --rounds 3
  $0 collect --rounds 5 --resume
  $0 validate
  $0 plot
EOF
        ;;
    *)
        echo "Unknown command: $MODE" >&2
        echo "Usage: $0 [all|collect|validate|plot] [collect options]" >&2
        exit 1
        ;;
esac
