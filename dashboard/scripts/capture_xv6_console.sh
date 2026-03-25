#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

CONSOLE_LOG="${DASHBOARD_RUNTIME_LOG:-dashboard/runtime/xv6-console.log}"
TELEMETRY_LOG="${DASHBOARD_TELEMETRY_LOG:-dashboard/runtime/dashboard-telemetry.log}"
SPLIT_SCRIPT="dashboard/scripts/split_console.py"
mkdir -p "$(dirname "$CONSOLE_LOG")"
: > "$CONSOLE_LOG"
: > "$TELEMETRY_LOG"

if [[ $# -gt 0 ]]; then
  RUN_CMD="$*"
else
  RUN_CMD="make CPUS=1 qemu"
fi

printf 'Interactive xv6 terminal with split telemetry\n'
printf '  terminal  : interactive output only (telemetry hidden)\n'
printf '  console   : %s\n' "$CONSOLE_LOG"
printf '  telemetry : %s\n' "$TELEMETRY_LOG"
printf '  command   : %s\n' "$RUN_CMD"
printf '\n'

exec python3 "$SPLIT_SCRIPT" \
  --console-log "$CONSOLE_LOG" \
  --telemetry-log "$TELEMETRY_LOG" \
  -- /bin/bash -lc "$RUN_CMD"
