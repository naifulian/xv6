#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

HOST="${DASHBOARD_HOST:-127.0.0.1}"
PORT="${DASHBOARD_PORT:-8000}"
SOURCE="${DASHBOARD_SOURCE:-dashboard/runtime/dashboard-telemetry.log}"
CONSOLE_LOG="${DASHBOARD_RUNTIME_LOG:-dashboard/runtime/xv6-console.log}"
CPUS="${DASHBOARD_CPUS:-1}"
MODE="telemetry"

extract_dashboardd_args() {
  sed -n '/^char \*dashboard_argv\[\] = {/,/^};/p' user/init.c \
    | tr -d '\n' \
    | sed -E 's/.*"dashboardd",[[:space:]]*"([^"]+)",[[:space:]]*"([^"]+)".*/\1 \2/'
}

print_usage() {
  printf 'Usage: bash dashboard/scripts/start_monitor.sh [--split-console] [command ...]\n'
  printf '\n'
  printf 'Default command\n'
  printf '  make CPUS=%s qemu\n' "$CPUS"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --split-console)
      MODE="split"
      shift
      ;;
    --help|-h)
      print_usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    *)
      break
      ;;
  esac
done

RUN_CMD=()
if [[ $# -gt 0 ]]; then
  RUN_CMD=("$@")
else
  RUN_CMD=(make "CPUS=$CPUS" qemu)
fi

mkdir -p dashboard/runtime
: > "$SOURCE"
if [[ "$MODE" == "split" ]]; then
  : > "$CONSOLE_LOG"
fi

DASHBOARDD_ARGS="$(extract_dashboardd_args)"
BRIDGE_PID=""

cleanup() {
  if [[ -n "$BRIDGE_PID" ]] && kill -0 "$BRIDGE_PID" 2>/dev/null; then
    kill "$BRIDGE_PID" 2>/dev/null || true
    wait "$BRIDGE_PID" 2>/dev/null || true
  fi
}

trap cleanup EXIT INT TERM

python3 -m dashboard.bridge.app --host "$HOST" --port "$PORT" --source "$SOURCE" --root dashboard &
BRIDGE_PID=$!
sleep 0.2

printf 'Module four monitor\n'
printf '  url      : http://%s:%s\n' "$HOST" "$PORT"
printf '  bridge   : pid=%s\n' "$BRIDGE_PID"
printf '  source   : %s\n' "$SOURCE"
printf '  mode     : %s\n' "$MODE"
printf '  dashboardd defaults : %s\n' "${DASHBOARDD_ARGS:-unknown}"
if [[ "$MODE" == "split" ]]; then
  printf '  console  : %s\n' "$CONSOLE_LOG"
  printf '  note     : split_console.py is fallback/debug only\n'
else
  printf '  note     : default path uses virtconsole telemetry side channel\n'
fi
printf '\n'
printf 'Foreground command\n'
printf '  %s\n' "${RUN_CMD[*]}"
printf '\n'

if [[ "$MODE" == "split" ]]; then
  bash dashboard/scripts/capture_xv6_console.sh "${RUN_CMD[@]}"
else
  "${RUN_CMD[@]}"
fi
