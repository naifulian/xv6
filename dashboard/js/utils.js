export const ROUTES = ["home", "processes", "scheduler", "memory", "events"];

export const ROUTE_META = {
  home: {
    label: "论文导览",
    copy: "首页先讲清模块关系，再跳到 live monitor 子页面。",
  },
  processes: {
    label: "进程管理",
    copy: "左侧看主表，右侧看选中进程的调度与地址空间解释。",
  },
  scheduler: {
    label: "调度观察",
    copy: "围绕当前策略展示真正值得讲解的观测字段与热点进程。",
  },
  memory: {
    label: "内存管理",
    copy: "先看系统页级口径，再看进程级地址空间与 VMA 细节。",
  },
  events: {
    label: "事件时间线",
    copy: "把状态变化、mmap 变化和 fault 变化从主表里解耦出来。",
  },
};

const STATE_PRIORITY = {
  RUNNING: 0,
  RUNNABLE: 1,
  SLEEPING: 2,
  ZOMBIE: 3,
  USED: 4,
  UNUSED: 5,
  UNKNOWN: 6,
};

export function emptySummary() {
  return {
    sample_count: 0,
    latest_seq: -1,
    latest_scheduler: "UNKNOWN",
    online: false,
    state_counts: {},
    system: {
      ticks: null,
      seq: null,
      policy: "UNKNOWN",
      cpu: null,
      context_switches: null,
      free_pages: null,
      runqueue: null,
      nr_total: null,
      nr_running: null,
      nr_sleeping: null,
      nr_zombie: null,
      cow_faults: null,
      lazy_allocs: null,
      cow_copy_pages: null,
      total_pages: null,
    },
    policy: {
      name: "UNKNOWN",
      label: "UNKNOWN",
      focus: "当前暂无遥测数据。",
      observed_field: "rutime",
      descending: true,
      metric_label: "运行时间",
    },
  };
}

export function emptySchedulerView() {
  return {
    cards: [],
    leaders: [],
    notes: [],
    state_breakdown: [],
    spotlight: {
      title: "等待遥测输入",
      detail: "桥接已启动，但还没有足够的 guest 快照。",
    },
    policy: emptySummary().policy,
    metric: "rutime",
    metric_label: "运行时间",
  };
}

export function emptyMemoryView() {
  return {
    totals: {
      total_pages: 0,
      free_pages: 0,
      used_pages: 0,
      pressure_percent: 0,
      cow_faults: 0,
      lazy_allocs: 0,
      cow_copy_pages: 0,
      total_vmas: 0,
      total_mmap_regions: 0,
      total_mmap_bytes: 0,
    },
    largest_process: null,
    notes: ["sz 表示逻辑地址空间规模，不等价于 RSS。"],
  };
}

export function formatNumber(value) {
  if (value == null || value === "") return "N/A";
  return `${value}`;
}

export function formatPercent(value) {
  if (value == null || Number.isNaN(Number(value))) return "N/A";
  return `${Number(value).toFixed(1)}%`;
}

export function formatTimeLabel(value) {
  if (!value) return "N/A";
  return value.replace("T", " ").slice(5, 19);
}

export function escapeHtml(value) {
  return `${value ?? ""}`
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

export function truncate(text, limit = 48) {
  const value = `${text ?? ""}`;
  if (value.length <= limit) return value;
  return `${value.slice(0, limit - 1)}...`;
}

export function stateClass(state) {
  const token = `${state || "unknown"}`.toLowerCase();
  return `state-pill ${token}`;
}

export function preferredProcess(processes, selectedPid = null) {
  const rows = Array.isArray(processes) ? processes : [];
  if (!rows.length) return null;
  if (selectedPid != null) {
    const found = rows.find((proc) => proc.pid === selectedPid);
    if (found) return found;
  }
  return rows
    .slice()
    .sort((a, b) => {
      const order = (STATE_PRIORITY[a.state] ?? 99) - (STATE_PRIORITY[b.state] ?? 99);
      if (order !== 0) return order;
      return Number(b.rutime || 0) - Number(a.rutime || 0);
    })[0];
}

export function statusLabel(bridgeOnline, summary) {
  if (!bridgeOnline) return "桥接离线";
  return summary.online ? "在线" : "等待遥测";
}

export function bindRouteButtons(container, actions) {
  container.querySelectorAll("[data-route]").forEach((button) => {
    button.addEventListener("click", () => actions.navigate(button.dataset.route));
  });
}
