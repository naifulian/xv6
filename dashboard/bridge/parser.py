"""Telemetry parsing helpers for the module four runtime dashboard."""

from __future__ import annotations

import json
import re
from datetime import datetime
from pathlib import Path
from typing import Any

SNAP_RE = re.compile(r"^SNAP\s+(.*)$")
PROC_RE = re.compile(r"^PROC\s+(.*)$")
VMA_RE = re.compile(r"^VMA\s+(.*)$")
FRAME_START_RE = re.compile(r"\b(?:SNAP|PROC|VMA|DASHBOARDD_BEGIN|DASHBOARDD_END|DASHBOARDD_ERROR)\b")
PAIR_RE = re.compile(r"(\w+)=([^\s]+)")
SYMBOL_CLEAN_RE = re.compile(r"[^A-Za-z0-9_.-]")

INT_FIELDS = {
    "seq",
    "ts",
    "policy",
    "cpu",
    "ctx",
    "ticks",
    "total_pages",
    "free_pages",
    "nr_total",
    "nr_running",
    "nr_sleeping",
    "nr_zombie",
    "runqueue",
    "buddy_merges",
    "buddy_splits",
    "cow_faults",
    "lazy_allocs",
    "cow_copy_pages",
    "pid",
    "priority",
    "tickets",
    "sched_class",
    "sz",
    "heap_end",
    "vma_count",
    "mmap_regions",
    "mmap_bytes",
    "slot",
    "start",
    "end",
    "length",
    "prot",
    "flags",
    "rutime",
    "retime",
    "stime",
}

POLICY_DETAILS = {
    "DEFAULT": {"label": "Round-Robin", "focus": "轮转公平性与上下文切换密度", "field": "rutime", "descending": True, "metric_label": "运行时间"},
    "FCFS": {"label": "First Come First Serve", "focus": "到达顺序与长作业占用情况", "field": "rutime", "descending": True, "metric_label": "运行时间"},
    "PRIORITY": {"label": "Priority", "focus": "优先级数值分布与等待压力", "field": "priority", "descending": False, "metric_label": "priority"},
    "SML": {"label": "Static Multi-Level", "focus": "静态层级差异与等待堆积", "field": "priority", "descending": False, "metric_label": "priority"},
    "LOTTERY": {"label": "Lottery", "focus": "tickets 分布与抽签池集中度", "field": "tickets", "descending": True, "metric_label": "tickets"},
    "SJF": {"label": "Shortest Job First", "focus": "短作业代理指标与等待代价", "field": "rutime", "descending": False, "metric_label": "已运行时间"},
    "SRTF": {"label": "Shortest Remaining Time First", "focus": "剩余时间未接入前的等待/运行代理观察", "field": "retime", "descending": True, "metric_label": "等待时间"},
    "MLFQ": {"label": "Multi-Level Feedback Queue", "focus": "交互性与等待时间分层", "field": "retime", "descending": True, "metric_label": "等待时间"},
    "CFS": {"label": "Completely Fair Scheduler", "focus": "运行时间分布与长期公平性", "field": "rutime", "descending": True, "metric_label": "运行时间"},
}


def ratio(part: int | float | None, total: int | float | None) -> float:
    if not total:
        return 0.0
    return float(part or 0) / float(total)


def numeric(value: Any) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        if isinstance(value, str):
            sign = "-" if value.startswith("-") else ""
            digits = "".join(ch for ch in value if ch.isdigit())
            if digits:
                return int(sign + digits)
        return 0


def parse_pairs(payload: str) -> dict[str, Any]:
    record: dict[str, Any] = {}
    for key, value in PAIR_RE.findall(payload):
        record[key] = numeric(value) if key in INT_FIELDS else SYMBOL_CLEAN_RE.sub("", value)
    return record


def split_frames(line: str) -> list[str]:
    positions = [match.start() for match in FRAME_START_RE.finditer(line)]
    if not positions:
        return []
    frames: list[str] = []
    for index, start in enumerate(positions):
        end = positions[index + 1] if index + 1 < len(positions) else len(line)
        frame = line[start:end].strip()
        if frame:
            frames.append(frame)
    return frames


def parse_lines(path: Path) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]]]:
    snapshots: list[dict[str, Any]] = []
    processes: list[dict[str, Any]] = []
    vmas: list[dict[str, Any]] = []

    for raw_line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = raw_line.strip()
        if not line:
            continue

        for frame in split_frames(line) or [line]:
            snap_match = SNAP_RE.search(frame)
            if snap_match:
                record = parse_pairs(snap_match.group(1))
                if "seq" in record:
                    snapshots.append(record)
                continue

            proc_match = PROC_RE.search(frame)
            if proc_match:
                record = parse_pairs(proc_match.group(1))
                if "seq" in record and "pid" in record:
                    processes.append(record)
                continue

            vma_match = VMA_RE.search(frame)
            if vma_match:
                record = parse_pairs(vma_match.group(1))
                if "seq" in record and "pid" in record:
                    vmas.append(record)

    snapshots.sort(key=lambda item: item["seq"])
    processes.sort(key=lambda item: (item["seq"], item["pid"]))
    vmas.sort(key=lambda item: (item["seq"], item["pid"], item.get("slot", 0), item.get("start", 0)))
    return snapshots, processes, vmas


def default_process(record: dict[str, Any]) -> dict[str, Any]:
    proc = dict(record)
    proc.setdefault("name", f"pid-{proc.get('pid', '?')}")
    proc.setdefault("state", "UNKNOWN")
    proc.setdefault("sched", "UNKNOWN")
    proc.setdefault("priority", 0)
    proc.setdefault("tickets", 0)
    proc.setdefault("sz", 0)
    proc.setdefault("heap_end", 0)
    proc.setdefault("vma_count", 0)
    proc.setdefault("mmap_regions", 0)
    proc.setdefault("mmap_bytes", 0)
    proc.setdefault("rutime", 0)
    proc.setdefault("retime", 0)
    proc.setdefault("stime", 0)
    return proc


def group_processes_by_seq(processes: list[dict[str, Any]]) -> dict[int, list[dict[str, Any]]]:
    grouped: dict[int, list[dict[str, Any]]] = {}
    for proc in processes:
        grouped.setdefault(proc["seq"], []).append(default_process(proc))
    return grouped


def group_vmas_by_seq(vmas: list[dict[str, Any]]) -> dict[int, dict[int, list[dict[str, Any]]]]:
    grouped: dict[int, dict[int, list[dict[str, Any]]]] = {}
    for vma in vmas:
        seq = numeric(vma.get("seq"))
        pid = numeric(vma.get("pid"))
        grouped.setdefault(seq, {}).setdefault(pid, []).append(dict(vma))
    for pid_map in grouped.values():
        for rows in pid_map.values():
            rows.sort(key=lambda row: (numeric(row.get("start")), numeric(row.get("slot"))))
    return grouped


def build_timelines(samples: list[dict[str, Any]]) -> dict[str, list[int]]:
    return {
        "cpu": [sample.get("cpu", 0) for sample in samples],
        "free_pages": [sample.get("free_pages", 0) for sample in samples],
        "runqueue": [sample.get("runqueue", 0) for sample in samples],
        "ticks": [sample.get("ticks", 0) for sample in samples],
        "context_switches": [sample.get("ctx", 0) for sample in samples],
    }


def build_summary(samples: list[dict[str, Any]], latest_processes: list[dict[str, Any]]) -> dict[str, Any]:
    latest = samples[-1] if samples else {}
    state_counts: dict[str, int] = {}
    for proc in latest_processes:
        state = proc.get("state", "UNKNOWN")
        state_counts[state] = state_counts.get(state, 0) + 1

    policy_name = latest.get("sched") or "UNKNOWN"
    policy_info = POLICY_DETAILS.get(policy_name, {"label": policy_name, "focus": "当前策略暂无额外说明", "field": "rutime", "descending": True, "metric_label": "运行时间"})

    return {
        "sample_count": len(samples),
        "latest_seq": latest.get("seq", -1),
        "latest_scheduler": latest.get("sched"),
        "online": bool(samples),
        "system": {
            "ticks": latest.get("ticks"),
            "seq": latest.get("seq"),
            "policy": policy_name,
            "cpu": latest.get("cpu"),
            "context_switches": latest.get("ctx"),
            "free_pages": latest.get("free_pages"),
            "runqueue": latest.get("runqueue"),
            "nr_total": latest.get("nr_total"),
            "nr_running": latest.get("nr_running"),
            "nr_sleeping": latest.get("nr_sleeping"),
            "nr_zombie": latest.get("nr_zombie"),
            "cow_faults": latest.get("cow_faults"),
            "lazy_allocs": latest.get("lazy_allocs"),
            "cow_copy_pages": latest.get("cow_copy_pages"),
            "total_pages": latest.get("total_pages"),
        },
        "state_counts": state_counts,
        "policy": {
            "name": policy_name,
            "label": policy_info["label"],
            "focus": policy_info["focus"],
            "observed_field": policy_info["field"],
            "descending": policy_info["descending"],
            "metric_label": policy_info["metric_label"],
        },
    }


def choose_selected_process(latest_processes: list[dict[str, Any]]) -> dict[str, Any] | None:
    if not latest_processes:
        return None
    preferred = sorted(latest_processes, key=lambda proc: (proc.get("state") != "RUNNING", proc.get("state") != "RUNNABLE", -(proc.get("rutime", 0)), proc.get("pid", 0)))
    return preferred[0]


def sort_processes(processes: list[dict[str, Any]], field: str, descending: bool) -> list[dict[str, Any]]:
    if descending:
        return sorted(processes, key=lambda proc: (numeric(proc.get(field)), numeric(proc.get("rutime")), -numeric(proc.get("pid"))), reverse=True)
    return sorted(processes, key=lambda proc: (numeric(proc.get(field)), -numeric(proc.get("rutime")), numeric(proc.get("pid"))))


def format_prot(prot: int) -> str:
    tokens: list[str] = []
    if prot & 0x1:
        tokens.append("R")
    if prot & 0x2:
        tokens.append("W")
    if prot & 0x4:
        tokens.append("X")
    return "".join(tokens) or "-"


def format_flags(flags: int) -> str:
    tokens: list[str] = []
    if flags & 0x02:
        tokens.append("PRIVATE")
    if flags & 0x20:
        tokens.append("ANON")
    return "|".join(tokens) or str(flags)


def build_vma_rows(vmas: list[dict[str, Any]]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for vma in sorted(vmas, key=lambda item: (numeric(item.get("start")), numeric(item.get("slot")))):
        start = numeric(vma.get("start"))
        end = numeric(vma.get("end"))
        length = numeric(vma.get("length"))
        prot = numeric(vma.get("prot"))
        flags = numeric(vma.get("flags"))
        rows.append({
            "slot": numeric(vma.get("slot")),
            "start": start,
            "end": end,
            "length": length,
            "pages": length // 4096 if length else 0,
            "prot": prot,
            "flags": flags,
            "prot_label": format_prot(prot),
            "flags_label": format_flags(flags),
            "range_label": f"{start} - {end}",
        })
    return rows


def build_process_detail(proc: dict[str, Any] | None, summary: dict[str, Any] | None = None, vmas: list[dict[str, Any]] | None = None) -> dict[str, Any] | None:
    if not proc:
        return None

    summary = summary or {}
    system = summary.get("system", {})
    policy = summary.get("policy", {})
    total_time = numeric(proc.get("rutime")) + numeric(proc.get("retime")) + numeric(proc.get("stime"))
    wait_share = round(ratio(proc.get("retime"), total_time) * 100, 1)
    sleep_share = round(ratio(proc.get("stime"), total_time) * 100, 1)
    run_share = round(ratio(proc.get("rutime"), total_time) * 100, 1)
    focus_field = policy.get("observed_field") or "rutime"
    vma_rows = build_vma_rows(vmas or [])

    return {
        "pid": proc.get("pid"),
        "name": proc.get("name"),
        "state": proc.get("state"),
        "sched": proc.get("sched") or policy.get("name") or "UNKNOWN",
        "priority": proc.get("priority"),
        "tickets": proc.get("tickets"),
        "sz": proc.get("sz"),
        "heap_end": proc.get("heap_end"),
        "vma_count": proc.get("vma_count"),
        "mmap_regions": proc.get("mmap_regions"),
        "mmap_bytes": proc.get("mmap_bytes"),
        "rutime": proc.get("rutime"),
        "retime": proc.get("retime"),
        "stime": proc.get("stime"),
        "total_time": total_time,
        "run_share": run_share,
        "wait_share": wait_share,
        "sleep_share": sleep_share,
        "focus_metric": focus_field,
        "focus_value": numeric(proc.get(focus_field)),
        "vmas": vma_rows,
        "scheduler_rows": [
            {"label": "当前策略", "value": proc.get("sched") or policy.get("label") or "UNKNOWN"},
            {"label": "焦点字段", "value": f"{policy.get('metric_label', focus_field)} ({focus_field})"},
            {"label": "字段值", "value": numeric(proc.get(focus_field))},
            {"label": "运行队列", "value": system.get("runqueue")},
        ],
        "memory_rows": [
            {"label": "逻辑地址空间 sz", "value": proc.get("sz")},
            {"label": "heap_end", "value": proc.get("heap_end")},
            {"label": "VMA 数", "value": proc.get("vma_count")},
            {"label": "mmap regions", "value": proc.get("mmap_regions")},
            {"label": "mmap bytes", "value": proc.get("mmap_bytes")},
            {"label": "系统 free pages", "value": system.get("free_pages")},
            {"label": "COW faults", "value": system.get("cow_faults")},
            {"label": "Lazy faults", "value": system.get("lazy_allocs")},
        ],
        "notes": [
            "sz 表示逻辑地址空间规模，不等价于 RSS。",
            "VMA 列表当前只展示匿名私有 mmap 快照，不展开页表级细节。",
            f"当前详情基于 {policy.get('label', 'UNKNOWN')} 的可用快照字段生成。",
        ],
    }


def build_scheduler_view(summary: dict[str, Any], latest_processes: list[dict[str, Any]]) -> dict[str, Any]:
    policy = summary["policy"]
    system = summary["system"]
    policy_name = policy["name"]
    field = policy["observed_field"]
    descending = bool(policy.get("descending", True))
    metric_label = policy.get("metric_label") or field
    ranked = sort_processes(latest_processes, field, descending)[:5]
    runnable = [proc for proc in latest_processes if proc.get("state") in {"RUNNING", "RUNNABLE"}]
    total_runtime = sum(numeric(proc.get("rutime")) for proc in latest_processes)
    total_metric = sum(max(0, numeric(proc.get(field))) for proc in latest_processes)
    top_runtime = max(latest_processes, key=lambda proc: numeric(proc.get("rutime")), default=None)
    top_waiter = max(latest_processes, key=lambda proc: numeric(proc.get("retime")), default=None)
    top_ticket = max(latest_processes, key=lambda proc: numeric(proc.get("tickets")), default=None)
    smallest_job = min(latest_processes, key=lambda proc: numeric(proc.get("rutime")), default=None)
    priority_values = [numeric(proc.get("priority")) for proc in latest_processes]
    priority_min = min(priority_values) if priority_values else None
    priority_max = max(priority_values) if priority_values else None
    priority_span = (priority_max - priority_min) if priority_min is not None and priority_max is not None else None

    cards: list[dict[str, Any]] = []
    spotlight = {"title": f"{policy.get('label', policy_name)} 观察摘要", "detail": f"焦点字段 {field}，当前 RUNNABLE/RUNNING 进程 {len(runnable)} 个。"}
    notes = [
        f"当前策略为 {policy_name}，主观察字段是 {field}。",
        f"运行队列长度 {system.get('runqueue', 0)}，上下文切换累计 {system.get('context_switches', 0)}。",
    ]

    if policy_name in {"DEFAULT", "FCFS"}:
        cards = [
            {"label": "上下文切换", "value": system.get("context_switches"), "description": "适合解释调度活跃度", "tone": "cool"},
            {"label": "运行队列", "value": system.get("runqueue"), "description": "当前可运行压力", "tone": "default"},
            {"label": "最活跃进程", "value": top_runtime.get("pid") if top_runtime else "N/A", "description": "按 rutime 观察占用时间", "tone": "warm"},
        ]
        if top_runtime:
            spotlight = {"title": "CPU 时间最集中的进程", "detail": f"pid={top_runtime.get('pid')} {top_runtime.get('name')} 的 rutime={top_runtime.get('rutime')}，适合用来解释长作业占用。"}
        notes.append("FCFS 更适合观察长作业占用与等待堆积，而不是切换频率。" if policy_name == "FCFS" else "Round-Robin 更适合观察上下文切换密度与运行时间分散程度。")
    elif policy_name in {"PRIORITY", "SML"}:
        cards = [
            {"label": "最小 priority", "value": priority_min, "description": "优先级数值下界", "tone": "warm"},
            {"label": "最大 priority", "value": priority_max, "description": "优先级数值上界", "tone": "default"},
            {"label": "priority 跨度", "value": priority_span, "description": "用于解释层级差异", "tone": "cool"},
        ]
        spotlight = {"title": "优先级分布快照", "detail": "当前页面只如实展示 priority 数值分布，不额外假设数值方向与内核实现含义。"}
        notes.append("queue_level 尚未接入时，先用 priority 与等待时间观察分层。")
    elif policy_name == "LOTTERY":
        top_share = round(ratio(top_ticket.get("tickets") if top_ticket else 0, total_metric) * 100, 1)
        cards = [
            {"label": "tickets 总量", "value": total_metric, "description": "抽签池规模", "tone": "cool"},
            {"label": "最高 tickets", "value": top_ticket.get("tickets") if top_ticket else 0, "description": "单进程持票上界", "tone": "warm"},
            {"label": "集中度", "value": f"{top_share}%", "description": "最高持票进程占比", "tone": "default"},
        ]
        if top_ticket:
            spotlight = {"title": "抽签池最集中的持票者", "detail": f"pid={top_ticket.get('pid')} {top_ticket.get('name')} 拥有 {top_ticket.get('tickets')} 张票，约占当前票池 {top_share}%。"}
        notes.append("Lottery 页面重点不在 CPU 时间，而在 ticket 分布是否过于集中。")
    elif policy_name in {"SJF", "SRTF"}:
        cards = [
            {"label": "最短已运行", "value": smallest_job.get("rutime") if smallest_job else 0, "description": "以 rutime 代理短作业", "tone": "cool"},
            {"label": "最长等待", "value": top_waiter.get("retime") if top_waiter else 0, "description": "等待压力热点", "tone": "warm"},
            {"label": "运行队列", "value": system.get("runqueue"), "description": "抢占候选规模", "tone": "default"},
        ]
        if top_waiter:
            spotlight = {"title": "等待时间热点", "detail": f"pid={top_waiter.get('pid')} {top_waiter.get('name')} 的 retime={top_waiter.get('retime')}，适合说明短作业策略下的等待代价。"}
        notes.append("remaining_time 尚未进入快照，本版用 rutime/retime 作为代理指标。")
    elif policy_name == "MLFQ":
        cards = [
            {"label": "最长等待", "value": top_waiter.get("retime") if top_waiter else 0, "description": "观察交互任务是否被压住", "tone": "warm"},
            {"label": "睡眠占优", "value": max((numeric(proc.get("stime")) for proc in latest_processes), default=0), "description": "粗略观察交互性", "tone": "cool"},
            {"label": "运行队列", "value": system.get("runqueue"), "description": "队列压力代理", "tone": "default"},
        ]
        spotlight = {"title": "分层行为代理观察", "detail": "queue_level 尚未接入，本版优先看等待时间与睡眠时间，帮助区分交互型和 CPU 密集型进程。"}
        notes.append("后续接入 queue_level 后，这里会切换成真正的多级队列视图。")
    elif policy_name == "CFS":
        runtime_values = [numeric(proc.get("rutime")) for proc in latest_processes]
        runtime_span = max(runtime_values) - min(runtime_values) if runtime_values else 0
        top_runtime_share = round(ratio(top_runtime.get("rutime") if top_runtime else 0, total_runtime) * 100, 1)
        cards = [
            {"label": "运行时间跨度", "value": runtime_span, "description": "越小通常越接近均衡", "tone": "cool"},
            {"label": "CPU 集中度", "value": f"{top_runtime_share}%", "description": "最高 rutime 占比", "tone": "warm"},
            {"label": "活跃进程", "value": len(runnable), "description": "当前参与竞争的进程数", "tone": "default"},
        ]
        if top_runtime:
            spotlight = {"title": "公平性代理观察", "detail": f"pid={top_runtime.get('pid')} {top_runtime.get('name')} 当前 rutime 最高，占比约 {top_runtime_share}%。"}
        notes.append("vruntime 尚未接入前，先用 rutime 分布近似观察长期公平性。")
    else:
        cards = [
            {"label": metric_label, "value": numeric(top_runtime.get(field) if top_runtime else 0), "description": "当前最显著的观察字段", "tone": "default"},
            {"label": "运行队列", "value": system.get("runqueue"), "description": "当前可运行压力", "tone": "cool"},
            {"label": "活跃进程", "value": len(runnable), "description": "RUNNING / RUNNABLE 总数", "tone": "warm"},
        ]

    leaders = []
    for proc in ranked:
        value = numeric(proc.get(field))
        share = round(ratio(value, total_metric) * 100, 1) if total_metric > 0 and field in {"rutime", "retime", "stime", "tickets", "sz"} else None
        leaders.append({
            "pid": proc.get("pid"),
            "name": proc.get("name"),
            "state": proc.get("state"),
            "value": value,
            "metric": field,
            "metric_label": metric_label,
            "share": share,
            "detail": f"rutime={proc.get('rutime', 0)} · retime={proc.get('retime', 0)} · stime={proc.get('stime', 0)}",
        })

    state_breakdown = [{"label": state, "value": count} for state, count in sorted(summary.get("state_counts", {}).items(), key=lambda item: (-item[1], item[0]))]

    return {
        "policy": policy,
        "metric": field,
        "metric_label": metric_label,
        "sort_descending": descending,
        "cards": cards,
        "spotlight": spotlight,
        "state_breakdown": state_breakdown,
        "leaders": leaders,
        "notes": notes,
    }


def build_memory_view(summary: dict[str, Any], latest_processes: list[dict[str, Any]]) -> dict[str, Any]:
    total_pages = summary["system"].get("total_pages") or 0
    free_pages = summary["system"].get("free_pages") or 0
    used_pages = max(0, total_pages - free_pages)
    pressure = round((used_pages * 100 / total_pages), 1) if total_pages else 0
    largest_proc = max(latest_processes, key=lambda proc: proc.get("sz", 0), default=None)
    total_vmas = sum(numeric(proc.get("vma_count")) for proc in latest_processes)
    total_mmap_regions = sum(numeric(proc.get("mmap_regions")) for proc in latest_processes)
    total_mmap_bytes = sum(numeric(proc.get("mmap_bytes")) for proc in latest_processes)

    return {
        "totals": {
            "total_pages": total_pages,
            "free_pages": free_pages,
            "used_pages": used_pages,
            "pressure_percent": pressure,
            "cow_faults": summary["system"].get("cow_faults", 0),
            "lazy_allocs": summary["system"].get("lazy_allocs", 0),
            "cow_copy_pages": summary["system"].get("cow_copy_pages", 0),
            "total_vmas": total_vmas,
            "total_mmap_regions": total_mmap_regions,
            "total_mmap_bytes": total_mmap_bytes,
        },
        "largest_process": build_process_detail(largest_proc, summary),
        "notes": [
            "sz 表示逻辑地址空间规模，不等价于 RSS。",
            "heap_end 表示当前堆顶，mmap 区域从独立 VMA 集合统计。",
            "当前内存面板聚焦 COW、Lazy Allocation 与逻辑地址空间口径。",
            "Buddy 计数保留在原始快照中，但不进入模块四主叙事。",
        ],
    }


def diff_events(samples: list[dict[str, Any]], processes_by_seq: dict[int, list[dict[str, Any]]], vmas_by_seq: dict[int, dict[int, list[dict[str, Any]]]]) -> list[dict[str, Any]]:
    events: list[dict[str, Any]] = []
    previous_sample = None
    previous_procs: dict[int, dict[str, Any]] = {}

    for sample in samples:
        seq = sample["seq"]
        current_list = processes_by_seq.get(seq, [])
        current_procs = {proc["pid"]: proc for proc in current_list}
        current_vmas = vmas_by_seq.get(seq, {})

        if previous_sample and sample.get("sched") != previous_sample.get("sched"):
            events.append({"seq": seq, "kind": "scheduler_switch", "title": "调度策略变化", "detail": f"{previous_sample.get('sched')} -> {sample.get('sched')}"})

        if previous_sample:
            for key, kind, title in [("cow_faults", "cow_fault", "COW fault 增长"), ("lazy_allocs", "lazy_fault", "Lazy fault 增长")]:
                delta = sample.get(key, 0) - previous_sample.get(key, 0)
                if delta > 0:
                    events.append({"seq": seq, "kind": kind, "title": title, "detail": f"+{delta} (累计 {sample.get(key, 0)})"})

        for pid, proc in current_procs.items():
            if pid not in previous_procs:
                events.append({"seq": seq, "kind": "fork", "title": "新进程出现", "detail": f"pid={pid} name={proc.get('name')}"})

        for pid, proc in previous_procs.items():
            if pid not in current_procs:
                events.append({"seq": seq, "kind": "exit", "title": "进程离开快照", "detail": f"pid={pid} name={proc.get('name')}"})

        for pid, proc in current_procs.items():
            old = previous_procs.get(pid)
            if old and old.get("state") != proc.get("state"):
                events.append({"seq": seq, "kind": "state_change", "title": "进程状态变化", "detail": f"pid={pid} {old.get('state')} -> {proc.get('state')}"})
            if old:
                prev_count = numeric(old.get("vma_count"))
                curr_count = numeric(proc.get("vma_count"))
                prev_bytes = numeric(old.get("mmap_bytes"))
                curr_bytes = numeric(proc.get("mmap_bytes"))
                if curr_count > prev_count or curr_bytes > prev_bytes:
                    events.append({"seq": seq, "kind": "mmap", "title": "mmap 区域增长", "detail": f"pid={pid} VMA {prev_count}->{curr_count} · bytes {prev_bytes}->{curr_bytes}"})
                elif curr_count < prev_count or curr_bytes < prev_bytes:
                    events.append({"seq": seq, "kind": "munmap", "title": "mmap 区域收缩", "detail": f"pid={pid} VMA {prev_count}->{curr_count} · bytes {prev_bytes}->{curr_bytes}"})
            rows = current_vmas.get(pid, [])
            if rows and pid not in previous_procs:
                first = rows[0]
                events.append({"seq": seq, "kind": "mmap", "title": "首次观测到 mmap 区域", "detail": f"pid={pid} slot={first.get('slot')} prot={format_prot(numeric(first.get('prot')))} flags={format_flags(numeric(first.get('flags')))}"})

        previous_sample = sample
        previous_procs = current_procs

    return events[-60:]


def parse_file(path: str | Path) -> dict[str, Any]:
    source = Path(path)
    if not source.exists():
        raise FileNotFoundError(source)

    snapshots, processes, vmas = parse_lines(source)
    processes_by_seq = group_processes_by_seq(processes)
    vmas_by_seq = group_vmas_by_seq(vmas)
    latest_seq = snapshots[-1]["seq"] if snapshots else -1
    latest_processes = processes_by_seq.get(latest_seq, [])
    latest_vmas = vmas_by_seq.get(latest_seq, {})
    summary = build_summary(snapshots, latest_processes)
    process_details = {str(proc.get("pid")): build_process_detail(proc, summary, latest_vmas.get(proc.get("pid"), [])) for proc in latest_processes}
    selected = choose_selected_process(latest_processes)
    selected_detail = process_details.get(str(selected.get("pid"))) if selected else None

    return {
        "generated_at": datetime.now().isoformat(),
        "source": str(source),
        "samples": snapshots,
        "latest_processes": latest_processes,
        "summary": summary,
        "timelines": build_timelines(snapshots),
        "selected_process": selected_detail,
        "process_details": process_details,
        "scheduler_view": build_scheduler_view(summary, latest_processes),
        "memory_view": build_memory_view(summary, latest_processes),
        "events": diff_events(snapshots, processes_by_seq, vmas_by_seq),
    }


def dump_payload(source: str | Path, target: str | Path) -> dict[str, Any]:
    payload = parse_file(source)
    target_path = Path(target)
    target_path.parent.mkdir(parents=True, exist_ok=True)
    target_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
    return payload
