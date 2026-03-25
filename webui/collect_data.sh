#!/bin/bash
# collect_data.sh - Collect module-three data with multi-round, resume, and split-suite support.

set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
LOG_DIR="${XV6_LOG_DIR:-$ROOT/log}"
BASELINE_ROOT="${BASELINE_ROOT:-}"
TESTING_ROOT="${TESTING_ROOT:-}"
TIMEOUT="${TIMEOUT:-1800}"
QEMU_CPUS="${QEMU_CPUS:-1}"
ROUNDS="${ROUNDS:-3}"
PERFTEST_CMD="${PERFTEST_CMD:-perftest all}"
CURRENT_BRANCH="$(git -C "$ROOT" branch --show-current 2>/dev/null || true)"
ACTIVE_BRANCH=""
RESUME=0
RUN_COMMANDS_PLANNED=0
RUN_COMMANDS_COMPLETED=0
RUN_FAILED_COMMAND=""
RUN_EXIT_CODE=0
MANIFEST_HEADER=$'run_id\tbranch\tcommit\tcollected_at\tcpus\ttimeout_s\tcommand\traw_log\tclean_log\tclean_sha256\tresult_count\trequired_results\tstatus\tsource_kind\trecovered_from_raw\tcommands_planned\tcommands_completed\tfailed_command\texit_code'

log_info() {
    echo "[INFO] $1"
}

log_warn() {
    echo "[WARN] $1"
}

log_error() {
    echo "[ERROR] $1" >&2
}

usage() {
    cat <<EOF
Usage: $0 [testing|baseline|all] [--rounds N] [--cpus N] [--timeout SEC] [--resume] [--cmd COMMAND]

Environment overrides:
  ROUNDS       Target number of complete rounds per branch (default: 3)
  QEMU_CPUS    QEMU CPU count (default: 1)
  TIMEOUT      Timeout per round in seconds (default: 1800)
  PERFTEST_CMD Command sent inside xv6 (default: perftest all)
  XV6_LOG_DIR  Log root for collected datasets (default: $ROOT/log)
  BASELINE_ROOT Alternate source root for baseline collection
  TESTING_ROOT Alternate source root for testing collection

Options:
  --cmd CMD    Override the xv6 command; default `perftest all` expands to separate memory/scheduler boots on testing
  --resume     Continue from existing runs instead of clearing the branch output
EOF
}

restore_branch() {
    if [ -n "$CURRENT_BRANCH" ] && [ -n "$ACTIVE_BRANCH" ] && [ "$CURRENT_BRANCH" != "$ACTIVE_BRANCH" ]; then
        git -C "$ROOT" checkout "$CURRENT_BRANCH" >/dev/null 2>&1 || true
    fi
}

cleanup() {
    rm -f /tmp/xv6_perftest.exp
    restore_branch
}

trap cleanup EXIT INT TERM

check_prerequisites() {
    if ! command -v expect >/dev/null 2>&1; then
        log_error "expect is not installed. Install with: sudo apt install expect"
        exit 1
    fi
    if ! command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
        log_error "RISC-V toolchain not found. Please install riscv64-unknown-elf-gcc"
        exit 1
    fi
    if ! command -v qemu-system-riscv64 >/dev/null 2>&1; then
        log_error "qemu-system-riscv64 not found"
        exit 1
    fi
}

hash_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    else
        shasum -a 256 "$1" | awk '{print $1}'
    fi
}

branch_source_root() {
    local branch="$1"

    case "$branch" in
        baseline)
            if [ -n "$BASELINE_ROOT" ]; then
                printf "%s\n" "$BASELINE_ROOT"
            else
                printf "%s\n" "$ROOT"
            fi
            ;;
        testing)
            if [ -n "$TESTING_ROOT" ]; then
                printf "%s\n" "$TESTING_ROOT"
            else
                printf "%s\n" "$ROOT"
            fi
            ;;
        *)
            printf "%s\n" "$ROOT"
            ;;
    esac
}

command_plan_for_branch() {
    local branch="$1"

    case "$PERFTEST_CMD" in
        "perftest all")
            if [ "$branch" = "testing" ]; then
                printf '%s\n' "perftest memory" "perftest sched_core" "perftest sched_fairness" "perftest sched_response"
            else
                printf '%s\n' "perftest memory"
            fi
            ;;
        *)
            printf '%s\n' "$PERFTEST_CMD"
            ;;
    esac
}

plan_description_for_branch() {
    local branch="$1"
    local plan=""

    while IFS= read -r command; do
        [ -n "$command" ] || continue
        if [ -z "$plan" ]; then
            plan="$command"
        else
            plan="$plan + $command"
        fi
    done < <(command_plan_for_branch "$branch")

    printf '%s\n' "$plan"
}

required_result_count() {
    local branch="$1"
    local required=0

    while IFS= read -r command; do
        case "$command" in
            "perftest memory")
                if [ "$branch" = "testing" ]; then
                    required=$((required + 12))
                else
                    required=$((required + 10))
                fi
                ;;
            "perftest sched")
                required=$((required + 13))
                ;;
            "perftest sched_core")
                required=$((required + 8))
                ;;
            "perftest sched_fairness")
                required=$((required + 5))
                ;;
            "perftest sched_response")
                required=$((required + 4))
                ;;
            "perftest all")
                if [ "$branch" = "testing" ]; then
                    required=$((required + 29))
                else
                    required=$((required + 10))
                fi
                ;;
        esac
    done < <(command_plan_for_branch "$branch")

    echo "$required"
}

planned_command_count() {
    local branch="$1"
    local count=0

    while IFS= read -r command; do
        [ -n "$command" ] || continue
        count=$((count + 1))
    done < <(command_plan_for_branch "$branch")

    echo "$count"
}

classify_run_status() {
    local branch="$1"
    local result_count="$2"
    local commands_planned="${3:-0}"
    local commands_completed="${4:-0}"
    local exit_code="${5:-0}"
    local required_count

    required_count=$(required_result_count "$branch")
    if [ "$result_count" -ge "$required_count" ] && [ "$commands_completed" -ge "$commands_planned" ] && [ "$exit_code" -eq 0 ]; then
        echo "complete"
    elif [ "$result_count" -gt 0 ] || [ "$commands_completed" -gt 0 ] || [ "$exit_code" -ne 0 ]; then
        echo "partial"
    else
        echo "empty"
    fi
}

run_index_from_id() {
    printf '%s\n' "$1" | sed -E 's/^run0*([0-9]+)$/\1/'
}

sanitize_raw_log() {
    local raw_log="$1"
    local clean_log="$2"
    local command_count=0

    command_count=$(grep -c '\$ perftest ' "$raw_log" 2>/dev/null || true)
    if [ "$command_count" -gt 1 ]; then
        grep -E '^(META|RESULT|SAMPLES):' "$raw_log" > "$clean_log" || true
    else
        awk '
            /\$ perftest / {capture=1}
            capture {print}
            capture && /Tests Complete/ {completed=1}
            completed && /^========================================$/ {separators++}
            completed && separators >= 2 {exit}
        ' "$raw_log" > "$clean_log" || true
    fi

    if ! grep -q '^RESULT:' "$clean_log" 2>/dev/null; then
        grep -E '^(META|RESULT|SAMPLES):' "$raw_log" > "$clean_log" || true
    fi

    if ! grep -q '^RESULT:' "$clean_log" 2>/dev/null; then
        cp "$raw_log" "$clean_log"
    fi
}

write_manifest_header() {
    local manifest="$1"
    printf '%s\n' "$MANIFEST_HEADER" > "$manifest"
}

ensure_manifest_header() {
    local manifest="$1"
    local current_header=""

    if [ -f "$manifest" ]; then
        current_header=$(head -n 1 "$manifest")
    fi

    if [ ! -f "$manifest" ] || [ "$current_header" != "$MANIFEST_HEADER" ]; then
        write_manifest_header "$manifest"
    fi
}

upsert_manifest_row() {
    local manifest="$1"
    local run_id="$2"
    local branch="$3"
    local commit="$4"
    local collected_at="$5"
    local cpus="$6"
    local timeout_s="$7"
    local command="$8"
    local raw_log="$9"
    local clean_log="${10}"
    local clean_sha="${11}"
    local result_count="${12}"
    local required_count="${13}"
    local status="${14}"
    local source_kind="${15}"
    local recovered_from_raw="${16}"
    local commands_planned="${17}"
    local commands_completed="${18}"
    local failed_command="${19}"
    local exit_code="${20}"
    local tmp="${manifest}.tmp"

    ensure_manifest_header "$manifest"
    awk -F'\t' -v run_id="$run_id" 'NR == 1 || $1 != run_id' "$manifest" > "$tmp"
    mv "$tmp" "$manifest"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$run_id" "$branch" "$commit" "$collected_at" "$cpus" "$timeout_s" "$command" \
        "$raw_log" "$clean_log" "$clean_sha" "$result_count" "$required_count" "$status" "$source_kind" "$recovered_from_raw" \
        "$commands_planned" "$commands_completed" "$failed_command" "$exit_code" >> "$manifest"
}

run_round_command() {
    local repo_root="$1"
    local raw_log="$2"
    local command="$3"
    local status=0

    cat > /tmp/xv6_perftest.exp <<EOF
#!/usr/bin/expect -f
set timeout $TIMEOUT
cd $repo_root
spawn make -C $repo_root CPUS=$QEMU_CPUS qemu
expect {
    "init: starting sh" { }
    timeout {
        puts "ERROR: Timeout waiting for shell"
        exit 1
    }
}
expect "\\$ "
send "$command\r"
expect {
    "Tests Complete" { }
    timeout {
        puts "ERROR: Timeout running perftest"
        exit 1
    }
}
expect "\\$ "
send "\\001x"
expect eof
EOF
    chmod +x /tmp/xv6_perftest.exp

    printf '[COLLECT] command=%s phase=begin at=%s\n' "$command" "$(date -Iseconds)" >> "$raw_log"
    if /tmp/xv6_perftest.exp >> "$raw_log" 2>&1; then
        status=0
        printf '[COLLECT] command=%s phase=end status=ok exit_code=%d at=%s\n' "$command" "$status" "$(date -Iseconds)" >> "$raw_log"
        return "$status"
    else
        status=$?
        printf '[COLLECT] command=%s phase=end status=failed exit_code=%d at=%s\n' "$command" "$status" "$(date -Iseconds)" >> "$raw_log"
        return "$status"
    fi
}

run_round() {
    local repo_root="$1"
    local raw_log="$2"
    local branch="$3"

    RUN_COMMANDS_PLANNED=0
    RUN_COMMANDS_COMPLETED=0
    RUN_FAILED_COMMAND=""
    RUN_EXIT_CODE=0
    : > "$raw_log"
    while IFS= read -r command; do
        [ -n "$command" ] || continue
        local command_status=0
        RUN_COMMANDS_PLANNED=$((RUN_COMMANDS_PLANNED + 1))
        if run_round_command "$repo_root" "$raw_log" "$command"; then
            command_status=0
            RUN_COMMANDS_COMPLETED=$((RUN_COMMANDS_COMPLETED + 1))
        else
            command_status=$?
            RUN_EXIT_CODE="$command_status"
            RUN_FAILED_COMMAND="$command"
            return "$RUN_EXIT_CODE"
        fi
    done < <(command_plan_for_branch "$branch")
}

prepare_branch_output() {
    local output_dir="$1"

    if [ "$RESUME" -eq 1 ]; then
        mkdir -p "$output_dir/runs"
        ensure_manifest_header "$output_dir/manifest.tsv"
        return
    fi

    rm -rf "$output_dir/runs"
    mkdir -p "$output_dir/runs"
    rm -f "$output_dir/perftest.log" "$output_dir/perftest_raw.log" "$output_dir/manifest.tsv"
    write_manifest_header "$output_dir/manifest.tsv"
}

sync_latest_logs() {
    local output_dir="$1"
    local runs_dir="$output_dir/runs"
    local latest_clean
    local latest_raw

    latest_clean=$(find "$runs_dir" -maxdepth 1 -type f -name 'run*_clean.log' -printf '%f\n' 2>/dev/null | sort | tail -n 1 || true)
    latest_raw=$(find "$runs_dir" -maxdepth 1 -type f -name 'run*_raw.log' -printf '%f\n' 2>/dev/null | sort | tail -n 1 || true)

    if [ -n "$latest_raw" ]; then
        cp "$runs_dir/$latest_raw" "$output_dir/perftest_raw.log"
    fi

    if [ -n "$latest_clean" ]; then
        cp "$runs_dir/$latest_clean" "$output_dir/perftest.log"
    elif [ -n "$latest_raw" ]; then
        sanitize_raw_log "$runs_dir/$latest_raw" "$output_dir/perftest.log"
    fi
}

command_progress_from_raw() {
    local raw_log="$1"
    local completed=0
    local exit_code=0
    local failed_command=""
    local failed_line=""

    if [ ! -f "$raw_log" ]; then
        printf '0\t0\t\n'
        return
    fi

    completed=$(tr -d '\r' < "$raw_log" | grep -c '^\[COLLECT\] command=.* phase=end status=ok' || true)
    failed_line=$({ tr -d '\r' < "$raw_log" | grep '^\[COLLECT\] command=.* phase=end status=failed' | tail -n 1; } || true)
    if [ -n "$failed_line" ]; then
        exit_code=$(printf '%s\n' "$failed_line" | sed -E 's/.* exit_code=([0-9]+) at=.*/\1/')
        failed_command=$(printf '%s\n' "$failed_line" | sed -E 's/^\[COLLECT\] command=(.*) phase=end status=failed exit_code=[0-9]+ at=.*$/\1/')
        if [ "$exit_code" -eq 0 ]; then
            exit_code=1
        fi
    fi

    printf '%s\t%s\t%s\n' "$completed" "$exit_code" "$failed_command"
}

scan_existing_runs() {
    local branch="$1"
    local output_dir="$2"
    local manifest="$3"
    local commit="$4"
    local runs_dir="$output_dir/runs"
    local required_count
    local commands_planned
    local complete_runs=0
    local max_index=0

    required_count=$(required_result_count "$branch")
    commands_planned=$(planned_command_count "$branch")
    ensure_manifest_header "$manifest"

    if [ ! -d "$runs_dir" ]; then
        echo "0 0"
        return
    fi

    while IFS= read -r run_id; do
        local raw_log="$runs_dir/${run_id}_raw.log"
        local clean_log="$runs_dir/${run_id}_clean.log"
        local source_log=""
        local source_kind=""
        local recovered_from_raw=0
        local collected_at
        local clean_sha=""
        local result_count=0
        local status
        local run_index
        local commands_completed=0
        local failed_command=""
        local exit_code=0
        local manifest_line=""
        local manifest_commit=""
        local manifest_collected_at=""
        local manifest_cpus=""
        local manifest_timeout=""
        local manifest_command=""
        local manifest_required_count=""
        local manifest_commands_planned=""

        [ -n "$run_id" ] || continue

        manifest_line=$(awk -F'\t' -v run_id="$run_id" 'NR > 1 && $1 == run_id { print; exit }' "$manifest")
        if [ -n "$manifest_line" ]; then
            IFS=$'\t' read -r _ _ manifest_commit manifest_collected_at manifest_cpus manifest_timeout manifest_command _ _ _ _ manifest_required_count _ _ _ manifest_commands_planned _ _ _ <<< "$manifest_line"
        fi

        if [ -f "$raw_log" ] && [ ! -f "$clean_log" ]; then
            sanitize_raw_log "$raw_log" "$clean_log"
            recovered_from_raw=1
        fi

        if [ -f "$clean_log" ]; then
            source_log="$clean_log"
            source_kind="clean"
            clean_sha=$(hash_file "$clean_log")
            result_count=$(grep -c '^RESULT:' "$clean_log" || true)
        elif [ -f "$raw_log" ]; then
            source_log="$raw_log"
            source_kind="raw"
            result_count=$(grep -c '^RESULT:' "$raw_log" || true)
        else
            continue
        fi

        if [ -f "$raw_log" ]; then
            IFS=$'\t' read -r commands_completed exit_code failed_command < <(command_progress_from_raw "$raw_log")
        fi
        if [ -n "$manifest_required_count" ]; then
            required_count="$manifest_required_count"
        fi
        if [ -n "$manifest_commands_planned" ]; then
            commands_planned="$manifest_commands_planned"
        fi
        if [ "$commands_completed" -eq 0 ] && [ "$result_count" -ge "$required_count" ]; then
            commands_completed="$commands_planned"
            exit_code=0
        fi

        status=$(classify_run_status "$branch" "$result_count" "$commands_planned" "$commands_completed" "$exit_code")
        collected_at="${manifest_collected_at:-$(date -Iseconds -r "$source_log" 2>/dev/null || date -Iseconds)}"
        upsert_manifest_row "$manifest" "$run_id" "$branch" "${manifest_commit:-$commit}" "$collected_at" \
            "${manifest_cpus:-$QEMU_CPUS}" "${manifest_timeout:-$TIMEOUT}" "${manifest_command:-$(plan_description_for_branch "$branch")}" \
            "$raw_log" "$clean_log" "$clean_sha" "$result_count" "$required_count" "$status" "$source_kind" "$recovered_from_raw" \
            "$commands_planned" "$commands_completed" "$failed_command" "$exit_code"

        if [ "$status" = "complete" ]; then
            complete_runs=$((complete_runs + 1))
        fi

        run_index=$(run_index_from_id "$run_id")
        if [ "$run_index" -gt "$max_index" ]; then
            max_index="$run_index"
        fi
    done < <(find "$runs_dir" -maxdepth 1 -type f \( -name 'run*_raw.log' -o -name 'run*_clean.log' \) -printf '%f\n' 2>/dev/null | sed -E 's/_(raw|clean)\.log$//' | sort -u)

    sync_latest_logs "$output_dir"
    echo "$complete_runs $max_index"
}

collect_branch() {
    local branch="$1"
    local repo_root
    local repo_branch
    local output_dir="$LOG_DIR/$branch"
    local runs_dir="$output_dir/runs"
    local manifest="$output_dir/manifest.tsv"
    local required_count
    local commands_planned
    local commit
    local complete_runs=0
    local max_index=0
    local next_index=1

    repo_root=$(branch_source_root "$branch")
    required_count=$(required_result_count "$branch")
    commands_planned=$(planned_command_count "$branch")
    prepare_branch_output "$output_dir"

    log_info "Collecting branch $branch with target $ROUNDS complete round(s)"
    log_info "Plan for $branch: $(plan_description_for_branch "$branch")"
    log_info "Source root for $branch: $repo_root"
    if [ "$RESUME" -eq 1 ]; then
        log_info "Resume mode is enabled; existing runs will be reused when possible"
    fi

    if [ "$repo_root" = "$ROOT" ]; then
        git -C "$ROOT" checkout "$branch" >/dev/null
        ACTIVE_BRANCH="$branch"
    else
        ACTIVE_BRANCH=""
        repo_branch="$(git -C "$repo_root" branch --show-current 2>/dev/null || true)"
        if [ -n "$repo_branch" ] && [ "$repo_branch" != "$branch" ]; then
            log_warn "[$branch] source root is on branch '$repo_branch'; collection label will still be '$branch'"
        fi
    fi

    make -C "$repo_root" clean >/dev/null 2>&1 || true
    make -C "$repo_root" CPUS="$QEMU_CPUS" >/dev/null 2>&1
    commit="$(git -C "$repo_root" rev-parse HEAD)"
    if ! git -C "$repo_root" diff --quiet --ignore-submodules HEAD -- 2>/dev/null; then
        commit="${commit}-dirty"
    fi

    if [ "$RESUME" -eq 1 ]; then
        read -r complete_runs max_index < <(scan_existing_runs "$branch" "$output_dir" "$manifest" "$commit")
        next_index=$((max_index + 1))
        if [ "$complete_runs" -ge "$ROUNDS" ]; then
            log_info "  [$branch] already has $complete_runs complete round(s); no new collection needed"
            return
        fi
    fi

    while [ "$complete_runs" -lt "$ROUNDS" ]; do
        local run_id
        local raw_log
        local clean_log
        local collected_at
        local clean_sha
        local result_count
        local status
        local run_rc=0

        run_id=$(printf 'run%02d' "$next_index")
        raw_log="$runs_dir/${run_id}_raw.log"
        clean_log="$runs_dir/${run_id}_clean.log"
        collected_at="$(date -Iseconds)"

        log_info "  [$branch] $run_id"
        if run_round "$repo_root" "$raw_log" "$branch"; then
            run_rc=0
        else
            run_rc=$?
            log_warn "  [$branch] $run_id stopped after command '$RUN_FAILED_COMMAND' (exit=$run_rc)"
        fi

        if [ -f "$raw_log" ]; then
            sanitize_raw_log "$raw_log" "$clean_log"
        else
            : > "$clean_log"
        fi

        clean_sha=""
        if [ -s "$clean_log" ]; then
            clean_sha="$(hash_file "$clean_log")"
        fi
        result_count="$(grep -c '^RESULT:' "$clean_log" || true)"
        status=$(classify_run_status "$branch" "$result_count" "$commands_planned" "$RUN_COMMANDS_COMPLETED" "$run_rc")
        upsert_manifest_row "$manifest" "$run_id" "$branch" "$commit" "$collected_at" \
            "$QEMU_CPUS" "$TIMEOUT" "$(plan_description_for_branch "$branch")" \
            "$raw_log" "$clean_log" "$clean_sha" "$result_count" "$required_count" "$status" "clean" "0" \
            "$commands_planned" "$RUN_COMMANDS_COMPLETED" "$RUN_FAILED_COMMAND" "$run_rc"
        sync_latest_logs "$output_dir"

        if [ "$status" != "complete" ]; then
            log_error "$branch $run_id produced only $result_count/$required_count result lines (commands $RUN_COMMANDS_COMPLETED/$commands_planned)"
            return 1
        fi

        complete_runs=$((complete_runs + 1))
        next_index=$((next_index + 1))
    done

    log_info "Finished $branch -> $output_dir"
}

TARGET=""
while [ $# -gt 0 ]; do
    case "$1" in
        testing|baseline|all)
            TARGET="$1"
            ;;
        --rounds)
            shift
            ROUNDS="$1"
            ;;
        --cpus)
            shift
            QEMU_CPUS="$1"
            ;;
        --timeout)
            shift
            TIMEOUT="$1"
            ;;
        --cmd)
            shift
            PERFTEST_CMD="$1"
            ;;
        --resume)
            RESUME=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            log_error "Unknown argument: $1"
            usage
            exit 1
            ;;
    esac
    shift
done

if [ -z "$TARGET" ]; then
    usage
    exit 1
fi

check_prerequisites
mkdir -p "$LOG_DIR"

case "$TARGET" in
    testing)
        collect_branch testing
        ;;
    baseline)
        collect_branch baseline
        ;;
    all)
        collect_branch testing
        collect_branch baseline
        ;;
esac

log_info "Data collection complete"
log_info "Validation: python3 webui/validate_logs.py"
log_info "Plotting:   python3 webui/plot_results.py"
