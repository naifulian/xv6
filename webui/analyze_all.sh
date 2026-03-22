#!/bin/bash
# analyze_all.sh - End-to-end module three automation
# Usage:
#   ./webui/analyze_all.sh all       # collect, validate, and plot
#   ./webui/analyze_all.sh collect   # collect logs from both branches
#   ./webui/analyze_all.sh validate  # validate existing logs
#   ./webui/analyze_all.sh plot      # generate figures and JSON summary

set -e

cmd=${1:-all}

case "$cmd" in
  collect)
    bash webui/collect_data.sh all
    ;;
  validate)
    python3 webui/validate_logs.py
    ;;
  plot)
    python3 webui/plot_results.py
    ;;
  all)
    bash webui/collect_data.sh all
    python3 webui/validate_logs.py
    python3 webui/plot_results.py
    ;;
  -h|--help)
    echo "Usage: $0 [all|collect|validate|plot]"
    ;;
  *)
    echo "Unknown command: $cmd"
    echo "Usage: $0 [all|collect|validate|plot]"
    exit 1
    ;;
esac
