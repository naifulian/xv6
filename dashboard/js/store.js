import { emptyMemoryView, emptySchedulerView, emptySummary, preferredProcess } from "./utils.js";

export function createStore() {
  return {
    route: "processes",
    bridgeOnline: false,
    meta: {
      generated_at: null,
      source: "dashboard/runtime/dashboard-telemetry.log",
      parse_error: null,
    },
    summary: emptySummary(),
    processes: [],
    selectedPid: null,
    selectedDetail: null,
    scheduler: emptySchedulerView(),
    memory: emptyMemoryView(),
    events: [],
  };
}

export function setRoute(state, route) {
  state.route = route;
}

export function mergeMeta(state, meta) {
  if (!meta) return;
  state.meta = {
    ...state.meta,
    ...meta,
  };
}

export function mergeSummary(state, summary) {
  if (!summary) return;
  state.summary = summary;
}

export function setProcesses(state, processes) {
  state.processes = Array.isArray(processes) ? processes : [];
  const focus = preferredProcess(state.processes, state.selectedPid);
  state.selectedPid = focus?.pid ?? null;
  if (!focus) {
    state.selectedDetail = null;
  } else if (state.selectedDetail?.pid !== focus.pid) {
    state.selectedDetail = null;
  }
}

export function setSelectedPid(state, pid) {
  state.selectedPid = pid;
  if (state.selectedDetail?.pid !== pid) {
    state.selectedDetail = null;
  }
}

export function setSelectedDetail(state, detail) {
  state.selectedDetail = detail || null;
  if (detail?.pid != null) {
    state.selectedPid = detail.pid;
  }
}

export function setScheduler(state, scheduler) {
  state.scheduler = scheduler || emptySchedulerView();
}

export function setMemory(state, memory) {
  state.memory = memory || emptyMemoryView();
}

export function setEvents(state, events) {
  state.events = Array.isArray(events) ? events : [];
}
