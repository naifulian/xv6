const REFRESH_PROFILE = {
  overview: 1000,
  process: 500,
  events: 200,
  static: 1000,
};


const statePriority = {
  RUNNING: 0,
  RUNNABLE: 1,
  SLEEPING: 2,
  ZOMBIE: 3,
  USED: 4,
  UNUSED: 5,
  UNKNOWN: 6,
};

const metricLabels = {
  rutime: "运行时间",
  retime: "等待时间",
  stime: "睡眠时间",
  priority: "priority",
  tickets: "tickets",
  sz: "逻辑地址空间",
};

let dashboardState = {
  payload: null,
  selectedPid: null,
  timers: {},
  mode: "loading",
  detailRequestId: 0,
};

const numeric = (value) => {
  const number = Number(value);
  return Number.isFinite(number) ? number : 0;
};

const formatNumber = (value) => (value == null || value === "" ? "N/A" : `${value}`);
const formatPercent = (value) => (value == null || Number.isNaN(Number(value)) ? "N/A" : `${Number(value).toFixed(1)}%`);

function emptyRawPayload() {
  return {
    generated_at: null,
    samples: [],
    latest_processes: [],
    summary: {},
    process_details: {},
    timelines: {},
    events: [],
  };
}

function currentPayload() {
  if (!dashboardState.payload) {
    dashboardState.payload = normalizePayload(emptyRawPayload());
  }
  return dashboardState.payload;
}

function applyFullPayload(raw) {
  const payload = normalizePayload(raw || emptyRawPayload());
  dashboardState.payload = payload;
  const processes = payload.latest_processes || [];
  if (dashboardState.selectedPid == null || !processes.some((proc) => proc.pid === dashboardState.selectedPid)) {
    dashboardState.selectedPid = payload.selected_process?.pid ?? processes[0]?.pid ?? null;
  }
  return dashboardState.payload;
}

function mergePayload(partial) {
  const current = currentPayload();
  const merged = {
    ...current,
    ...partial,
    summary: partial.summary
      ? {
          ...current.summary,
          ...partial.summary,
          system: {
            ...(current.summary?.system || {}),
            ...(partial.summary.system || {}),
          },
          policy: {
            ...(current.summary?.policy || {}),
            ...(partial.summary.policy || {}),
          },
          state_counts: partial.summary.state_counts || current.summary?.state_counts || {},
        }
      : current.summary,
    scheduler_view: partial.scheduler_view
      ? {
          ...(current.scheduler_view || {}),
          ...partial.scheduler_view,
          policy: {
            ...(current.scheduler_view?.policy || {}),
            ...(partial.scheduler_view.policy || {}),
          },
        }
      : current.scheduler_view,
    memory_view: partial.memory_view
      ? {
          ...(current.memory_view || {}),
          ...partial.memory_view,
          totals: {
            ...(current.memory_view?.totals || {}),
            ...(partial.memory_view.totals || {}),
          },
        }
      : current.memory_view,
    timelines: partial.timelines ? { ...(current.timelines || {}), ...partial.timelines } : current.timelines,
    process_details: partial.process_details
      ? { ...(current.process_details || {}), ...partial.process_details }
      : current.process_details,
  };

  if (!("selected_process" in partial)) merged.selected_process = current.selected_process;
  if (!("events" in partial)) merged.events = current.events;
  if (!("latest_processes" in partial)) merged.latest_processes = current.latest_processes;
  if (!("samples" in partial)) merged.samples = current.samples;
  if (!("generated_at" in partial)) merged.generated_at = current.generated_at;

  return applyFullPayload(merged);
}

function clearRefreshTimers() {
  Object.values(dashboardState.timers).forEach((timer) => clearInterval(timer));
  dashboardState.timers = {};
}

function renderRefreshProfile() {
  const mode = dashboardState.mode;
  const refreshMode = document.getElementById("refresh-mode");
  const overview = document.getElementById("overview-refresh");
  const process = document.getElementById("process-refresh");
  const events = document.getElementById("events-refresh");

  if (!refreshMode || !overview || !process || !events) {
    return;
  }

  if (mode === "api") {
    refreshMode.textContent = "分层";
    overview.textContent = `${(REFRESH_PROFILE.overview / 1000).toFixed(1)}s`;
    process.textContent = `${(REFRESH_PROFILE.process / 1000).toFixed(1)}s`;
    events.textContent = `${(REFRESH_PROFILE.events / 1000).toFixed(1)}s`;
    return;
  }

  refreshMode.textContent = "静态";
  overview.textContent = `${(REFRESH_PROFILE.static / 1000).toFixed(1)}s`;
  process.textContent = "跟随静态源";
  events.textContent = "跟随静态源";
}

async function loadJson(path, fallback) {
  try {
    const res = await fetch(path, { cache: "no-store" });
    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }
    return await res.json();
  } catch (_) {
    return fallback;
  }
}

function sparkline(points, color) {
  if (!points.length) {
    return '<p class="note">暂无快照数据</p>';
  }

  const width = 320;
  const height = 140;
  const padding = 12;
  const max = Math.max(...points);
  const min = Math.min(...points);
  const span = Math.max(1, max - min);
  const coords = points
    .map((value, index) => {
      const x = padding + (index * (width - padding * 2)) / Math.max(1, points.length - 1);
      const y = height - padding - ((value - min) * (height - padding * 2)) / span;
      return `${x},${y}`;
    })
    .join(" ");

  return `
    <svg viewBox="0 0 ${width} ${height}" preserveAspectRatio="none">
      <rect x="0" y="0" width="${width}" height="${height}" rx="18" fill="rgba(10, 28, 36, 0.05)"></rect>
      <polyline points="${coords}" fill="none" stroke="${color}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"></polyline>
    </svg>
    <div class="axis-labels"><span>${min}</span><span>${max}</span></div>
  `;
}

function deriveEvents(samples, latestProcesses) {
  if (!samples.length && !latestProcesses.length) {
    return [];
  }

  const latest = samples[samples.length - 1] || {};
  return [
    {
      seq: latest.seq ?? "-",
      title: "最新快照已加载",
      detail: `policy=${latest.sched || "UNKNOWN"} runqueue=${latest.runqueue ?? "N/A"}`,
      kind: "info",
    },
    ...latestProcesses.slice(0, 5).map((proc) => ({
      seq: latest.seq ?? "-",
      title: "进程出现在主表",
      detail: `pid=${proc.pid} state=${proc.state} sched=${proc.sched}`,
      kind: "process",
    })),
  ];
}

function deriveStateCounts(latestProcesses) {
  const counts = {};
  latestProcesses.forEach((proc) => {
    const state = proc.state || "UNKNOWN";
    counts[state] = (counts[state] || 0) + 1;
  });
  return counts;
}

function buildFallbackSchedulerView(summary, latestProcesses) {
  const stateCounts = summary.state_counts || deriveStateCounts(latestProcesses);
  const policy = summary.policy || { name: "UNKNOWN", label: "UNKNOWN", focus: "当前策略暂无额外说明", observed_field: "rutime", descending: true };
  const metric = policy.observed_field || "rutime";
  const sortDescending = policy.descending ?? true;
  const ordered = latestProcesses
    .slice()
    .sort((a, b) => {
      const primary = sortDescending ? numeric(b[metric]) - numeric(a[metric]) : numeric(a[metric]) - numeric(b[metric]);
      if (primary !== 0) return primary;
      return numeric(b.rutime) - numeric(a.rutime);
    });

  return {
    policy,
    metric,
    metric_label: policy.metric_label || metricLabels[metric] || metric,
    sort_descending: sortDescending,
    cards: [
      {
        label: "运行队列",
        value: summary.system?.runqueue ?? "N/A",
        description: "当前可运行压力",
        tone: "default",
      },
      {
        label: "上下文切换",
        value: summary.system?.context_switches ?? "N/A",
        description: "调度活跃度代理",
        tone: "cool",
      },
      {
        label: "活跃进程",
        value: summary.system?.nr_running ?? "N/A",
        description: "RUNNING / RUNNABLE 总数",
        tone: "warm",
      },
    ],
    spotlight: {
      title: `${policy.label || policy.name || "UNKNOWN"} 观察摘要`,
      detail: policy.focus || "当前策略暂无额外说明。",
    },
    state_breakdown: Object.entries(stateCounts)
      .sort((a, b) => b[1] - a[1])
      .map(([label, value]) => ({ label, value })),
    leaders: ordered.slice(0, 5).map((proc) => ({
      pid: proc.pid,
      name: proc.name,
      state: proc.state,
      metric,
      metric_label: policy.metric_label || metricLabels[metric] || metric,
      value: numeric(proc[metric]),
      detail: `rutime=${numeric(proc.rutime)} · retime=${numeric(proc.retime)} · stime=${numeric(proc.stime)}`,
    })),
    notes: [
      `当前策略为 ${policy.name || "UNKNOWN"}。`,
      `RUNNABLE / RUNNING 进程 ${summary.system?.nr_running ?? "N/A"} 个，运行队列 ${summary.system?.runqueue ?? "N/A"}。`,
    ],
  };
}

function normalizePayload(raw) {
  const payload = raw || {};
  const samples = Array.isArray(payload.samples) ? payload.samples : [];
  const latestProcesses = Array.isArray(payload.latest_processes) ? payload.latest_processes : [];
  const latest = samples[samples.length - 1] || {};
  const summary = payload.summary || {};
  const system = summary.system || {};
  const derivedStateCounts = summary.state_counts || deriveStateCounts(latestProcesses);

  const normalizedSummary = {
    sample_count: summary.sample_count ?? samples.length,
    latest_seq: summary.latest_seq ?? latest.seq ?? -1,
    latest_scheduler: summary.latest_scheduler ?? latest.sched ?? "UNKNOWN",
    online: summary.online ?? samples.length > 0,
    state_counts: derivedStateCounts,
    system: {
      ticks: system.ticks ?? latest.ticks ?? null,
      seq: system.seq ?? latest.seq ?? null,
      policy: system.policy ?? latest.sched ?? "UNKNOWN",
      cpu: system.cpu ?? latest.cpu ?? null,
      context_switches: system.context_switches ?? latest.ctx ?? null,
      free_pages: system.free_pages ?? latest.free_pages ?? null,
      runqueue: system.runqueue ?? latest.runqueue ?? null,
      nr_total: system.nr_total ?? latest.nr_total ?? latestProcesses.length,
      nr_running: system.nr_running ?? latest.nr_running ?? latestProcesses.filter((proc) => ["RUNNING", "RUNNABLE"].includes(proc.state)).length,
      nr_sleeping: system.nr_sleeping ?? latest.nr_sleeping ?? latestProcesses.filter((proc) => proc.state === "SLEEPING").length,
      nr_zombie: system.nr_zombie ?? latest.nr_zombie ?? latestProcesses.filter((proc) => proc.state === "ZOMBIE").length,
      cow_faults: system.cow_faults ?? latest.cow_faults ?? 0,
      lazy_allocs: system.lazy_allocs ?? latest.lazy_allocs ?? 0,
      cow_copy_pages: system.cow_copy_pages ?? latest.cow_copy_pages ?? 0,
      total_pages: system.total_pages ?? latest.total_pages ?? null,
    },
    policy: summary.policy || {
      name: latest.sched || "UNKNOWN",
      label: latest.sched || "UNKNOWN",
      focus: "当前策略暂无额外说明",
      observed_field: "rutime",
      descending: true,
      metric_label: "运行时间",
    },
  };

  const selectedProcess =
    payload.selected_process ||
    latestProcesses
      .slice()
      .sort((a, b) => {
        const orderA = statePriority[a.state] ?? 99;
        const orderB = statePriority[b.state] ?? 99;
        if (orderA !== orderB) return orderA - orderB;
        return numeric(b.rutime) - numeric(a.rutime);
      })[0] ||
    null;

  const fallbackSchedulerView = buildFallbackSchedulerView(normalizedSummary, latestProcesses);
  const rawSchedulerView = payload.scheduler_view || {};
  const schedulerView = {
    policy: rawSchedulerView.policy || fallbackSchedulerView.policy,
    metric: rawSchedulerView.metric || fallbackSchedulerView.metric,
    metric_label: rawSchedulerView.metric_label || fallbackSchedulerView.metric_label,
    sort_descending: rawSchedulerView.sort_descending ?? fallbackSchedulerView.sort_descending,
    cards: Array.isArray(rawSchedulerView.cards) && rawSchedulerView.cards.length ? rawSchedulerView.cards : fallbackSchedulerView.cards,
    spotlight: rawSchedulerView.spotlight || fallbackSchedulerView.spotlight,
    state_breakdown: Array.isArray(rawSchedulerView.state_breakdown) && rawSchedulerView.state_breakdown.length ? rawSchedulerView.state_breakdown : fallbackSchedulerView.state_breakdown,
    leaders: Array.isArray(rawSchedulerView.leaders) ? rawSchedulerView.leaders : fallbackSchedulerView.leaders,
    notes: Array.isArray(rawSchedulerView.notes) && rawSchedulerView.notes.length ? rawSchedulerView.notes : fallbackSchedulerView.notes,
  };

  const totalPages = normalizedSummary.system.total_pages ?? 0;
  const freePages = normalizedSummary.system.free_pages ?? 0;
  const totalVmas = latestProcesses.reduce((sum, proc) => sum + numeric(proc.vma_count), 0);
  const totalMmapRegions = latestProcesses.reduce((sum, proc) => sum + numeric(proc.mmap_regions), 0);
  const totalMmapBytes = latestProcesses.reduce((sum, proc) => sum + numeric(proc.mmap_bytes), 0);
  const largestProcess = latestProcesses.slice().sort((a, b) => numeric(b.sz) - numeric(a.sz))[0] || null;
  const memoryView = payload.memory_view || {
    totals: {
      total_pages: totalPages,
      free_pages: freePages,
      used_pages: Math.max(0, totalPages - freePages),
      pressure_percent: totalPages ? ((totalPages - freePages) * 100) / totalPages : 0,
      cow_faults: normalizedSummary.system.cow_faults,
      lazy_allocs: normalizedSummary.system.lazy_allocs,
      cow_copy_pages: normalizedSummary.system.cow_copy_pages,
      total_vmas: totalVmas,
      total_mmap_regions: totalMmapRegions,
      total_mmap_bytes: totalMmapBytes,
    },
    largest_process: largestProcess,
    notes: [
      "sz 表示逻辑地址空间规模，不等价于 RSS。",
      "当前仅基于 getsnapshot/getptable/getmemstat 生成视图。",
    ],
  };

  const processDetails = payload.process_details || (selectedProcess ? { [selectedProcess.pid]: selectedProcess } : {});

  return {
    generated_at: payload.generated_at || null,
    samples,
    latest_processes: latestProcesses,
    summary: normalizedSummary,
    selected_process: selectedProcess,
    process_details: processDetails,
    scheduler_view: schedulerView,
    memory_view: memoryView,
    timelines: payload.timelines || {
      cpu: samples.map((item) => item.cpu || 0),
      free_pages: samples.map((item) => item.free_pages || 0),
      runqueue: samples.map((item) => item.runqueue || 0),
      ticks: samples.map((item) => item.ticks || 0),
    },
    events: Array.isArray(payload.events) && payload.events.length ? payload.events : deriveEvents(samples, latestProcesses),
  };
}

function createKpi(title, value, extra, tone = "default") {
  const card = document.createElement("article");
  card.className = `kpi-card ${tone}`;
  card.innerHTML = `<p>${title}</p><strong>${value}</strong><em>${extra}</em>`;
  return card;
}

function renderHeader(payload) {
  const summary = payload.summary;
  const system = summary.system;
  document.getElementById("live-status").textContent = summary.online ? "在线" : "离线";
  document.getElementById("policy-name").textContent = system.policy || "UNKNOWN";
  document.getElementById("seq-label").textContent = formatNumber(system.seq);
  document.getElementById("generated-at").textContent = payload.generated_at ? payload.generated_at.replace("T", " ").slice(5, 19) : "N/A";
  renderRefreshProfile();
}

function renderKpis(payload) {
  const grid = document.getElementById("kpi-grid");
  const system = payload.summary.system;
  const memory = payload.memory_view.totals || {};
  grid.innerHTML = "";

  [
    ["总进程数", formatNumber(system.nr_total), "进程表中的最新快照", "default"],
    ["RUNNABLE / RUNNING", formatNumber(system.nr_running), "调度压力核心指标", "warm"],
    ["运行队列", formatNumber(system.runqueue), "当前可运行长度", "cool"],
    ["Free Pages", formatNumber(system.free_pages), "空闲物理页数量", "cool"],
    ["COW Faults", formatNumber(system.cow_faults), "写时复制 fault 累计", "warm"],
    ["Lazy Faults", formatNumber(system.lazy_allocs), "按需分配 fault 累计", "warm"],
    ["Context Switches", formatNumber(system.context_switches), "上下文切换累计", "default"],
    ["Memory Pressure", formatPercent(memory.pressure_percent), "按 total/free pages 估算", "default"],
  ].forEach(([title, value, extra, tone]) => grid.appendChild(createKpi(title, value, extra, tone)));
}

function renderProcessTable(payload) {
  const tbody = document.getElementById("process-table");
  const processes = payload.latest_processes.slice().sort((a, b) => {
    const stateOrder = (statePriority[a.state] ?? 99) - (statePriority[b.state] ?? 99);
    if (stateOrder !== 0) return stateOrder;
    return numeric(b.rutime) - numeric(a.rutime);
  });

  tbody.innerHTML = "";
  if (!processes.length) {
    const row = document.createElement("tr");
    row.innerHTML = '<td colspan="10" class="note">暂无进程快照。先运行 dashboardd，或用 capture 脚本把 xv6 控制台写到 runtime 日志。</td>';
    tbody.appendChild(row);
    return;
  }

  const selectedPid = dashboardState.selectedPid ?? payload.selected_process?.pid ?? processes[0].pid;
  dashboardState.selectedPid = selectedPid;

  processes.forEach((proc) => {
    const row = document.createElement("tr");
    if (proc.pid === selectedPid) row.classList.add("selected");
    row.innerHTML = `
      <td>${formatNumber(proc.pid)}</td>
      <td>${proc.name || "unknown"}</td>
      <td><span class="state-pill">${proc.state || "UNKNOWN"}</span></td>
      <td>${proc.sched || "UNKNOWN"}</td>
      <td>${formatNumber(proc.priority)}</td>
      <td>${formatNumber(proc.tickets)}</td>
      <td>${formatNumber(proc.rutime)}</td>
      <td>${formatNumber(proc.retime)}</td>
      <td>${formatNumber(proc.stime)}</td>
      <td>${formatNumber(proc.sz)}</td>
    `;
    row.addEventListener("click", () => {
      dashboardState.selectedPid = proc.pid;
      renderProcessTable(payload);
      renderProcessDetail(payload);
      renderMemory(payload);
      if (dashboardState.mode === "api") {
        refreshSelectedProcessDetail();
      }
    });
    tbody.appendChild(row);
  });
}

function getSelectedProcess(payload) {
  const base = payload.latest_processes.find((proc) => proc.pid === dashboardState.selectedPid) || payload.selected_process || null;
  if (!base) return null;
  const detail = payload.process_details?.[base.pid] || (payload.selected_process?.pid === base.pid ? payload.selected_process : null);
  return detail ? { ...base, ...detail } : base;
}

function appendInfoRow(container, label, value) {
  const item = document.createElement("div");
  item.className = "info-row";
  item.innerHTML = `<span>${label}</span><strong>${formatNumber(value)}</strong>`;
  container.appendChild(item);
}

function createSection(title, description = "") {
  const section = document.createElement("section");
  section.className = "detail-section";
  section.innerHTML = `<div class="detail-section-head"><h3>${title}</h3>${description ? `<p>${description}</p>` : ""}</div>`;
  return section;
}

function deriveProcessInsight(proc, payload) {
  const totalTime = numeric(proc.rutime) + numeric(proc.retime) + numeric(proc.stime);
  const runShare = totalTime ? (numeric(proc.rutime) * 100) / totalTime : 0;
  const waitShare = totalTime ? (numeric(proc.retime) * 100) / totalTime : 0;
  const sleepShare = totalTime ? (numeric(proc.stime) * 100) / totalTime : 0;
  const view = payload.scheduler_view || {};
  const metric = view.metric || payload.summary.policy?.observed_field || "rutime";
  const metricLabel = view.metric_label || payload.summary.policy?.metric_label || metricLabels[metric] || metric;
  const sortDescending = view.sort_descending ?? payload.summary.policy?.descending ?? true;
  const ordered = payload.latest_processes
    .slice()
    .sort((a, b) => {
      const primary = sortDescending ? numeric(b[metric]) - numeric(a[metric]) : numeric(a[metric]) - numeric(b[metric]);
      if (primary !== 0) return primary;
      return numeric(b.rutime) - numeric(a.rutime);
    });
  const metricRank = ordered.findIndex((item) => item.pid === proc.pid) + 1;
  const totalSz = payload.latest_processes.reduce((sum, item) => sum + numeric(item.sz), 0);
  const sizeShare = totalSz ? (numeric(proc.sz) * 100) / totalSz : 0;
  const stateCounts = payload.summary.state_counts || deriveStateCounts(payload.latest_processes);
  const largestProcess = payload.memory_view?.largest_process;
  const notes = [
    `当前按 ${metricLabel} (${metric}) 在主表中排序位置约为 ${metricRank || "N/A"}/${payload.latest_processes.length || 0}。`,
    `系统当前共有 ${stateCounts.RUNNING || 0} 个 RUNNING、${stateCounts.RUNNABLE || 0} 个 RUNNABLE 进程。`,
    "sz 表示逻辑地址空间规模，不等价于 RSS。",
  ];
  if (largestProcess && largestProcess.pid === proc.pid) {
    notes.unshift("当前选中进程是本次快照里 sz 最大的进程。");
  }
  if ((proc.state || "UNKNOWN") === "RUNNING") {
    notes.unshift("当前选中进程正在占用 CPU。可以结合 rutime 与 runqueue 一起讲解调度行为。");
  }

  return {
    totalTime,
    runShare,
    waitShare,
    sleepShare,
    quickCards: [
      { label: "priority", value: formatNumber(proc.priority), note: "静态调度属性" },
      { label: "VMA", value: formatNumber(proc.vma_count), note: "当前 mmap 区域数" },
      { label: "mmap bytes", value: formatNumber(proc.mmap_bytes), note: "匿名私有映射规模" },
      { label: "总时长", value: formatNumber(totalTime), note: "rutime + retime + stime" },
    ],
    schedulerRows: [
      ["当前策略", proc.sched || payload.summary.policy?.label || "UNKNOWN"],
      ["焦点字段", `${metricLabel} (${metric})`],
      ["字段值", formatNumber(proc[metric])],
      ["同策略排序", metricRank ? `${metricRank}/${payload.latest_processes.length}` : "N/A"],
      ["运行队列", formatNumber(payload.summary.system.runqueue)],
    ],
    memoryRows: [
      ["逻辑地址空间 sz", formatNumber(proc.sz)],
      ["heap_end", formatNumber(proc.heap_end)],
      ["VMA 数", formatNumber(proc.vma_count)],
      ["mmap regions", formatNumber(proc.mmap_regions)],
      ["mmap bytes", formatNumber(proc.mmap_bytes)],
      ["sz 占全表比例", formatPercent(sizeShare)],
      ["系统 free pages", formatNumber(payload.summary.system.free_pages)],
      ["Memory Pressure", formatPercent(payload.memory_view?.totals?.pressure_percent)],
      ["COW Faults", formatNumber(payload.summary.system.cow_faults)],
      ["Lazy Faults", formatNumber(payload.summary.system.lazy_allocs)],
    ],
    notes,
  };
}

function renderProcessDetail(payload) {
  const container = document.getElementById("process-detail");
  container.innerHTML = "";
  const proc = getSelectedProcess(payload);

  if (!proc) {
    container.innerHTML = '<p class="note">暂无可展示的进程详情。</p>';
    return;
  }

  const insight = deriveProcessInsight(proc, payload);

  const hero = document.createElement("div");
  hero.className = "detail-hero";
  hero.innerHTML = `<p>PID ${proc.pid}</p><h3>${proc.name || "unknown"}</h3><span>${proc.state || "UNKNOWN"} · ${proc.sched || "UNKNOWN"}</span>`;
  container.appendChild(hero);

  const quickGrid = document.createElement("div");
  quickGrid.className = "detail-card-grid";
  insight.quickCards.forEach((card) => {
    const item = document.createElement("article");
    item.className = "detail-mini-card";
    item.innerHTML = `<span>${card.label}</span><strong>${card.value}</strong><p>${card.note}</p>`;
    quickGrid.appendChild(item);
  });
  container.appendChild(quickGrid);

  const runtimeSection = createSection("时间分解", "把运行、等待和睡眠拆开，适合现场解释当前进程的状态特征。");
  const runtimeRows = document.createElement("div");
  runtimeRows.className = "info-stack";
  [["rutime", proc.rutime], ["retime", proc.retime], ["stime", proc.stime], ["total time", insight.totalTime]].forEach(([label, value]) => appendInfoRow(runtimeRows, label, value));
  runtimeSection.appendChild(runtimeRows);

  const shares = document.createElement("div");
  shares.className = "bar-group";
  [["运行占比", insight.runShare, "var(--accent-red)"], ["等待占比", insight.waitShare, "var(--accent-gold)"], ["睡眠占比", insight.sleepShare, "var(--accent-teal)"]].forEach(([label, value, color]) => {
    const item = document.createElement("div");
    item.className = "bar-item";
    item.innerHTML = `<div class="bar-label"><span>${label}</span><strong>${formatPercent(value)}</strong></div><div class="bar-track"><div class="bar-fill" style="width:${Math.max(6, numeric(value))}%; background:${color};"></div></div>`;
    shares.appendChild(item);
  });
  runtimeSection.appendChild(shares);
  container.appendChild(runtimeSection);

  const schedSection = createSection("调度摘要", "这里展示当前策略真正会用来讲解的观测字段，而不是只堆原始列。");
  const schedRows = document.createElement("div");
  schedRows.className = "info-stack";
  insight.schedulerRows.forEach(([label, value]) => appendInfoRow(schedRows, label, value));
  schedSection.appendChild(schedRows);
  container.appendChild(schedSection);

  const memorySection = createSection("地址空间摘要", "仍然坚持诚实口径，只展示逻辑地址空间和 fault 相关观测。");
  const memoryRows = document.createElement("div");
  memoryRows.className = "info-stack";
  insight.memoryRows.forEach(([label, value]) => appendInfoRow(memoryRows, label, value));
  memorySection.appendChild(memoryRows);
  container.appendChild(memorySection);

  const vmaSection = createSection("VMA 列表", "帮助区分 heap_end 以内的堆空间和匿名私有 mmap 区域。当前仅展示最新快照。");
  const vmas = Array.isArray(proc.vmas) ? proc.vmas : [];
  if (!vmas.length) {
    const empty = document.createElement("p");
    empty.className = "note";
    empty.textContent = "当前选中进程没有可展示的 VMA；这通常表示还未发生匿名私有 mmap。";
    vmaSection.appendChild(empty);
  } else {
    const list = document.createElement("div");
    list.className = "vma-list";
    vmas.forEach((vma) => {
      const item = document.createElement("article");
      item.className = "vma-card";
      item.innerHTML = `
        <div class="vma-topline">
          <strong>slot ${formatNumber(vma.slot)}</strong>
          <span>${vma.prot_label || "-"} · ${vma.flags_label || "-"}</span>
        </div>
        <p>${vma.range_label || "N/A"}</p>
        <div class="vma-meta">
          <span>bytes ${formatNumber(vma.length)}</span>
          <span>pages ${formatNumber(vma.pages)}</span>
        </div>
      `;
      list.appendChild(item);
    });
    vmaSection.appendChild(list);
  }
  container.appendChild(vmaSection);

  const notes = document.createElement("div");
  notes.className = "detail-notes";
  insight.notes.forEach((note) => {
    const item = document.createElement("p");
    item.className = "note";
    item.textContent = note;
    notes.appendChild(item);
  });
  container.appendChild(notes);
}

function renderScheduler(payload) {
  const policyCard = document.getElementById("policy-card");
  const leaders = document.getElementById("scheduler-leaders");
  const view = payload.scheduler_view;
  policyCard.innerHTML = "";
  leaders.innerHTML = "";

  const summary = document.createElement("div");
  summary.className = "policy-summary";
  summary.innerHTML = `
    <p class="policy-kicker">${view.policy?.label || payload.summary.system.policy || "UNKNOWN"}</p>
    <h3>${view.policy?.focus || "当前策略暂无额外说明"}</h3>
    <p class="note">焦点字段：${view.metric_label || metricLabels[view.metric] || view.metric || "rutime"} (${view.metric || "rutime"})</p>
  `;
  policyCard.appendChild(summary);

  if (view.spotlight) {
    const spotlight = document.createElement("article");
    spotlight.className = "policy-spotlight";
    spotlight.innerHTML = `<strong>${view.spotlight.title || "观察摘要"}</strong><p>${view.spotlight.detail || ""}</p>`;
    policyCard.appendChild(spotlight);
  }

  if ((view.cards || []).length) {
    const grid = document.createElement("div");
    grid.className = "policy-card-grid";
    view.cards.forEach((card) => {
      const item = document.createElement("article");
      item.className = `policy-metric-card ${card.tone || "default"}`;
      item.innerHTML = `<span>${card.label}</span><strong>${formatNumber(card.value)}</strong><p>${card.description || ""}</p>`;
      grid.appendChild(item);
    });
    policyCard.appendChild(grid);
  }

  if ((view.state_breakdown || []).length) {
    const strip = document.createElement("div");
    strip.className = "state-breakdown";
    view.state_breakdown.forEach((item) => {
      const token = document.createElement("div");
      token.className = "state-token";
      token.innerHTML = `<span>${item.label}</span><strong>${formatNumber(item.value)}</strong>`;
      strip.appendChild(token);
    });
    policyCard.appendChild(strip);
  }

  (view.notes || []).forEach((note) => {
    const p = document.createElement("p");
    p.className = "note";
    p.textContent = note;
    policyCard.appendChild(p);
  });

  if (!(view.leaders || []).length) {
    leaders.innerHTML = '<p class="note">暂无调度器专属观察对象。</p>';
    return;
  }

  const caption = document.createElement("p");
  caption.className = "leader-caption";
  caption.textContent = `${view.metric_label || metricLabels[view.metric] || view.metric || "字段"} 排名前列`;
  leaders.appendChild(caption);

  const maxValue = Math.max(...view.leaders.map((item) => numeric(item.value)), 1);
  view.leaders.forEach((item) => {
    const card = document.createElement("div");
    card.className = "leader-card rich";
    const width = item.share != null ? Math.max(8, numeric(item.share)) : Math.max(8, (numeric(item.value) * 100) / maxValue);
    card.innerHTML = `
      <div class="leader-topline">
        <div>
          <strong>${item.name || "unknown"}</strong>
          <span>pid=${item.pid} · ${item.state || "UNKNOWN"}</span>
        </div>
        <b>${item.metric || view.metric || "metric"}=${formatNumber(item.value)}</b>
      </div>
      <div class="leader-meter"><div class="leader-meter-bar" style="width:${width}%;"></div></div>
      <p class="leader-detail">${item.detail || ""}</p>
    `;
    leaders.appendChild(card);
  });
}

function renderMemory(payload) {
  const summary = document.getElementById("memory-summary");
  const notes = document.getElementById("memory-notes");
  const view = payload.memory_view;
  const totals = view.totals || {};
  const selected = getSelectedProcess(payload);

  summary.innerHTML = "";
  notes.innerHTML = "";

  [["total pages", totals.total_pages], ["free pages", totals.free_pages], ["used pages", totals.used_pages], ["pressure", formatPercent(totals.pressure_percent)], ["VMA 总数", totals.total_vmas], ["mmap regions", totals.total_mmap_regions], ["mmap bytes", totals.total_mmap_bytes], ["cow faults", totals.cow_faults], ["lazy faults", totals.lazy_allocs], ["cow copy pages", totals.cow_copy_pages]].forEach(([label, value]) => appendInfoRow(summary, label, value));

  if (view.largest_process) {
    const p = document.createElement("p");
    p.className = "note";
    p.textContent = `当前 sz 最大进程: pid=${view.largest_process.pid} ${view.largest_process.name} (${view.largest_process.sz})`;
    summary.appendChild(p);
  }

  if (selected) {
    const p = document.createElement("p");
    p.className = "note";
    p.textContent = `当前选中进程: pid=${selected.pid} sz=${formatNumber(selected.sz)}，heap_end=${formatNumber(selected.heap_end)}，VMA=${formatNumber(selected.vma_count)}。`;
    summary.appendChild(p);
  }

  (view.notes || []).forEach((note) => {
    const item = document.createElement("p");
    item.className = "note";
    item.textContent = note;
    notes.appendChild(item);
  });
}

function renderTrends(payload) {
  const timelines = payload.timelines || {};
  document.getElementById("cpu-chart").innerHTML = sparkline(timelines.cpu || [], "#cf5b3f");
  document.getElementById("mem-chart").innerHTML = sparkline(timelines.free_pages || [], "#0f7d7a");
  document.getElementById("rq-chart").innerHTML = sparkline(timelines.runqueue || [], "#2147aa");
  document.getElementById("ticks-chart").innerHTML = sparkline(timelines.ticks || [], "#9d7418");
}

function buildEventSummary(events) {
  const recent = (events || []).slice(-12);
  const exits = recent.filter((event) => event.kind === "exit").slice(-3).reverse();
  return {
    recentCount: recent.length,
    exitCount: recent.filter((event) => event.kind === "exit").length,
    forkCount: recent.filter((event) => event.kind === "fork").length,
    lastEvent: recent[recent.length - 1] || null,
    exits,
  };
}

function renderEventSummary(payload) {
  const container = document.getElementById("event-summary");
  if (!container) {
    return;
  }

  const summary = buildEventSummary(payload.events);
  container.innerHTML = "";

  [
    ["最近事件", formatNumber(summary.recentCount), "近 12 条活动"],
    ["最近 fork", formatNumber(summary.forkCount), "短时创建行为"],
    ["最近退出", formatNumber(summary.exitCount), summary.lastEvent ? `最后事件: ${summary.lastEvent.title || "事件"}` : "等待新事件"],
  ].forEach(([label, value, note]) => {
    const card = document.createElement("article");
    card.className = "event-summary-card";
    card.innerHTML = `<span>${label}</span><strong>${value}</strong><p>${note}</p>`;
    container.appendChild(card);
  });

  if (summary.exits.length) {
    const strip = document.createElement("div");
    strip.className = "recent-exit-strip";
    summary.exits.forEach((event) => {
      const item = document.createElement("div");
      item.className = "recent-exit-token";
      item.textContent = `seq ${event.seq ?? "-"} · ${event.detail || "exit"}`;
      strip.appendChild(item);
    });
    container.appendChild(strip);
  }
}

function renderEvents(payload) {
  const container = document.getElementById("event-list");
  const events = (payload.events || []).slice().reverse();
  container.innerHTML = "";

  if (!events.length) {
    container.innerHTML = '<p class="note">暂无事件。等采样链路稳定后，这里会显示快照差分生成的调度和内存事件。</p>';
    return;
  }

  events.forEach((event) => {
    const item = document.createElement("article");
    item.className = `event-card ${event.kind || "info"}`;
    item.innerHTML = `
      <div class="event-meta">
        <span>seq ${event.seq ?? "-"}</span>
        <strong>${event.title || "事件"}</strong>
      </div>
      <p>${event.detail || ""}</p>
    `;
    container.appendChild(item);
  });
}

function renderOverviewLayer(payload) {
  renderHeader(payload);
  renderKpis(payload);
  renderTrends(payload);
}

function renderProcessLayer(payload) {
  renderProcessTable(payload);
  renderProcessDetail(payload);
  renderScheduler(payload);
  renderMemory(payload);
}

function renderEventLayer(payload) {
  renderEventSummary(payload);
  renderEvents(payload);
}

function renderDashboard(payload) {
  renderOverviewLayer(payload);
  renderProcessLayer(payload);
  renderEventLayer(payload);
}

async function refreshSelectedProcessDetail() {
  if (dashboardState.mode !== "api" || dashboardState.selectedPid == null) {
    return;
  }

  const requestId = ++dashboardState.detailRequestId;
  const detail = await loadJson(`/api/processes/${dashboardState.selectedPid}`, null);
  if (!detail || requestId !== dashboardState.detailRequestId) {
    return;
  }

  const payload = mergePayload({
    process_details: { [dashboardState.selectedPid]: detail },
    selected_process: detail,
  });
  renderProcessLayer(payload);
}

async function updateOverviewLayer() {
  if (dashboardState.mode === "static") {
    const payload = applyFullPayload(await loadJson("data/snapshots.json", emptyRawPayload()));
    renderDashboard(payload);
    return;
  }

  const raw = await loadJson("/api/dashboard", null);
  if (!raw) {
    return;
  }

  const payload = applyFullPayload(raw);
  renderDashboard(payload);
}

async function updateProcessLayer() {
  if (dashboardState.mode !== "api") {
    return;
  }

  const processes = await loadJson("/api/processes", null);
  if (!Array.isArray(processes)) {
    return;
  }

  const visible = new Set(processes.map((proc) => proc.pid));
  if (dashboardState.selectedPid == null || !visible.has(dashboardState.selectedPid)) {
    dashboardState.selectedPid = processes[0]?.pid ?? null;
  }

  const payload = mergePayload({ latest_processes: processes });
  renderProcessLayer(payload);
  await refreshSelectedProcessDetail();
}

async function updateEventLayer() {
  if (dashboardState.mode !== "api") {
    return;
  }

  const events = await loadJson("/api/events", null);
  if (!Array.isArray(events)) {
    return;
  }

  const payload = mergePayload({ events });
  renderEventLayer(payload);
}

function startApiRefresh() {
  clearRefreshTimers();
  dashboardState.timers.overview = setInterval(updateOverviewLayer, REFRESH_PROFILE.overview);
  dashboardState.timers.process = setInterval(updateProcessLayer, REFRESH_PROFILE.process);
  dashboardState.timers.events = setInterval(updateEventLayer, REFRESH_PROFILE.events);
}

function startStaticRefresh() {
  clearRefreshTimers();
  dashboardState.timers.static = setInterval(updateOverviewLayer, REFRESH_PROFILE.static);
}

async function main() {
  const apiPayload = await loadJson("/api/dashboard", null);
  if (apiPayload) {
    dashboardState.mode = "api";
    const payload = applyFullPayload(apiPayload);
    renderDashboard(payload);
    await refreshSelectedProcessDetail();
    startApiRefresh();
  } else {
    dashboardState.mode = "static";
    const payload = applyFullPayload(await loadJson("data/snapshots.json", emptyRawPayload()));
    renderDashboard(payload);
    startStaticRefresh();
  }
}

window.addEventListener("beforeunload", clearRefreshTimers);
main();
