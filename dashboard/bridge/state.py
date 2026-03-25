"""Cached runtime state for the module four host bridge."""

from __future__ import annotations

from pathlib import Path
from threading import Lock
from typing import Any

from .parser import parse_file


class DashboardState:
    def __init__(self, source_path: str | Path):
        self.source_path = Path(source_path)
        self._lock = Lock()
        self._mtime_ns: int | None = None
        self._payload: dict[str, Any] | None = None

    def get_payload(self) -> dict[str, Any]:
        with self._lock:
            current_mtime = self.source_path.stat().st_mtime_ns if self.source_path.exists() else None
            if self._payload is None or current_mtime != self._mtime_ns:
                self._mtime_ns = current_mtime
                if self.source_path.exists():
                    try:
                        self._payload = parse_file(self.source_path)
                    except Exception as exc:
                        payload = dict(self._payload or self._empty_payload())
                        payload["parse_error"] = str(exc)
                        self._payload = payload
                else:
                    self._payload = self._empty_payload()
            return self._payload

    def _empty_payload(self) -> dict[str, Any]:
        return {
            "generated_at": None,
            "source": str(self.source_path),
            "samples": [],
            "latest_processes": [],
            "summary": {
                "sample_count": 0,
                "latest_seq": -1,
                "latest_scheduler": None,
                "online": False,
                "system": {
                    "ticks": None,
                    "seq": None,
                    "policy": "UNKNOWN",
                    "cpu": None,
                    "context_switches": None,
                    "free_pages": None,
                    "runqueue": None,
                    "nr_total": None,
                    "nr_running": None,
                    "nr_sleeping": None,
                    "nr_zombie": None,
                    "cow_faults": None,
                    "lazy_allocs": None,
                    "cow_copy_pages": None,
                    "total_pages": None,
                },
                "state_counts": {},
                "policy": {
                    "name": "UNKNOWN",
                    "label": "UNKNOWN",
                    "focus": "当前策略暂无额外说明",
                    "observed_field": "rutime",
                    "descending": True,
                    "metric_label": "运行时间",
                },
            },
            "timelines": {"cpu": [], "free_pages": [], "runqueue": [], "ticks": [], "context_switches": []},
            "selected_process": None,
            "process_details": {},
            "scheduler_view": {
                "policy": {
                    "name": "UNKNOWN",
                    "label": "UNKNOWN",
                    "focus": "当前策略暂无额外说明",
                    "observed_field": "rutime",
                    "descending": True,
                    "metric_label": "运行时间",
                },
                "metric": "rutime",
                "metric_label": "运行时间",
                "sort_descending": True,
                "cards": [],
                "spotlight": {
                    "title": "等待遥测输入",
                    "detail": "请先运行 dashboardd 或将 SNAP/PROC 帧写入宿主机日志。",
                },
                "state_breakdown": [],
                "leaders": [],
                "notes": [
                    "当前没有可用遥测数据。",
                    "请先运行 sysmon 或 dashboardd 并将日志写入宿主机文件。",
                ],
            },
            "memory_view": {
                "totals": {
                    "total_pages": 0,
                    "free_pages": 0,
                    "used_pages": 0,
                    "pressure_percent": 0,
                    "cow_faults": None,
                    "lazy_allocs": None,
                    "cow_copy_pages": None,
                    "total_vmas": 0,
                    "total_mmap_regions": 0,
                    "total_mmap_bytes": 0,
                },
                "largest_process": None,
                "notes": [
                    "sz 表示逻辑地址空间规模，不等价于 RSS。",
                    "当前没有可用遥测数据。",
                ],
            },
            "events": [],
        }
