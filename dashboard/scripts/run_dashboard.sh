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

printf 'Module four dashboard bridge (bridge only)\n'
printf '  host   : %s\n' "$HOST"
printf '  port   : %s\n' "$PORT"
printf '  source : %s\n' "$SOURCE"
printf '\n'
printf 'Recommended entry\n'
printf '  bash dashboard/scripts/start_monitor.sh\n'
printf '\n'
printf 'Advanced paths\n'
printf '  bridge only : bash dashboard/scripts/run_dashboard.sh\n'
printf '  fallback PTY split : bash dashboard/scripts/capture_xv6_console.sh\n'
printf '\n'
printf 'Open http://%s:%s\n' "$HOST" "$PORT"

exec python3 -m dashboard.bridge.app --host "$HOST" --port "$PORT" --source "$SOURCE" --root dashboard
