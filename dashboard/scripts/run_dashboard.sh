#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

HOST="${DASHBOARD_HOST:-127.0.0.1}"
PORT="${DASHBOARD_PORT:-8000}"
SOURCE="${DASHBOARD_SOURCE:-dashboard/runtime/dashboard-telemetry.log}"

if [[ $# -ge 1 ]]; then
  SOURCE="$1"
fi
if [[ $# -ge 2 ]]; then
  PORT="$2"
fi

mkdir -p dashboard/runtime
touch "$SOURCE"

printf 'Module four dashboard bridge\n'
printf '  host   : %s\n' "$HOST"
printf '  port   : %s\n' "$PORT"
printf '  source : %s\n' "$SOURCE"
printf '\n'
printf 'Next steps\n'
printf '  1. Terminal A: bash dashboard/scripts/run_dashboard.sh\n'
printf '  2. Terminal B: bash dashboard/scripts/capture_xv6_console.sh  # shell stays interactive, telemetry is split out\n'
printf '  3. xv6 init auto-starts dashboardd 30 0\n'
printf '\n'
printf 'Open http://%s:%s\n' "$HOST" "$PORT"

exec python3 -m dashboard.bridge.app --host "$HOST" --port "$PORT" --source "$SOURCE" --root dashboard
