import { loadEvents, loadMemory, loadMeta, loadProcessDetail, loadProcesses, loadScheduler, loadSummary } from "./api.js";
import { createRouter } from "./router.js";
import { createStore, mergeMeta, mergeSummary, setEvents, setMemory, setProcesses, setRoute, setScheduler, setSelectedDetail, setSelectedPid } from "./store.js";
import { renderEvents } from "./views/events.js";
import { renderHome } from "./views/home.js";
import { renderMemory } from "./views/memory.js";
import { renderProcesses } from "./views/processes.js";
import { renderScheduler } from "./views/scheduler.js";
import { ROUTE_META, formatNumber, formatTimeLabel, statusLabel } from "./utils.js";

const state = createStore();
const viewRoot = document.getElementById("view-root");
const shellRefs = {
  sidebar: document.querySelector(".sidebar"),
  workspace: document.querySelector(".workspace"),
  liveStatus: document.getElementById("live-status"),
  policyName: document.getElementById("policy-name"),
  seqLabel: document.getElementById("seq-label"),
  generatedAt: document.getElementById("generated-at"),
  telemetrySource: document.getElementById("telemetry-source"),
  routeLabel: document.getElementById("route-label"),
  telemetryWarning: document.getElementById("telemetry-warning"),
  navLinks: Array.from(document.querySelectorAll("[data-route-link]")),
};

let routeTimer = null;
let routeRequestId = 0;
let lastRoute = state.route;

const routeIntervals = {
  home: 1000,
  processes: 500,
  scheduler: 700,
  memory: 800,
  events: 250,
};

const routeViews = {
  home: renderHome,
  processes: renderProcesses,
  scheduler: renderScheduler,
  memory: renderMemory,
  events: renderEvents,
};

let router = null;

const actions = {
  navigate(route) {
    router.go(route);
  },
  async selectProcess(pid) {
    setSelectedPid(state, pid);
    renderCurrentRoute();
    await refreshSelectedProcess();
  },
};

function renderShell() {
  const meta = ROUTE_META[state.route] || ROUTE_META.processes;
  shellRefs.liveStatus.textContent = statusLabel(state.bridgeOnline, state.summary);
  shellRefs.policyName.textContent = formatNumber(state.summary.system?.policy || state.summary.policy?.name || "UNKNOWN");
  shellRefs.seqLabel.textContent = formatNumber(state.summary.system?.seq);
  shellRefs.generatedAt.textContent = formatTimeLabel(state.meta.generated_at);
  shellRefs.telemetrySource.textContent = state.meta.source || "dashboard/runtime/dashboard-telemetry.log";
  shellRefs.routeLabel.textContent = meta.label;
  document.title = `${meta.label} - xv6 运行监控`;

  shellRefs.navLinks.forEach((link) => {
    link.classList.toggle("active", link.dataset.routeLink === state.route);
  });

  if (state.meta.parse_error) {
    shellRefs.telemetryWarning.textContent = `bridge 解析告警：${state.meta.parse_error}`;
    shellRefs.telemetryWarning.classList.remove("hidden");
  } else {
    shellRefs.telemetryWarning.textContent = "";
    shellRefs.telemetryWarning.classList.add("hidden");
  }
}

function renderCurrentRoute() {
  const render = routeViews[state.route] || routeViews.processes;
  render(viewRoot, state, actions);
}

function resetShellScroll(force = false) {
  if (!force && state.route === lastRoute) return;
  shellRefs.sidebar?.scrollTo({ top: 0, left: 0, behavior: "auto" });
  shellRefs.workspace?.scrollTo({ top: 0, left: 0, behavior: "auto" });
  lastRoute = state.route;
}

async function refreshShell() {
  const [meta, summary] = await Promise.all([loadMeta(), loadSummary()]);
  state.bridgeOnline = Boolean(meta || summary);
  mergeMeta(state, meta);
  mergeSummary(state, summary);
  renderShell();
  renderCurrentRoute();
}

async function refreshSelectedProcess() {
  if (state.selectedPid == null) return;
  const detail = await loadProcessDetail(state.selectedPid);
  if (detail?.pid === state.selectedPid) {
    setSelectedDetail(state, detail);
    renderCurrentRoute();
  }
}

async function loadHome() {
  const [memory, events, processes] = await Promise.all([loadMemory(), loadEvents(), loadProcesses()]);
  setMemory(state, memory);
  setEvents(state, events);
  setProcesses(state, processes);
}

async function loadProcessesView() {
  const processes = await loadProcesses();
  setProcesses(state, processes);
  await refreshSelectedProcess();
}

async function loadSchedulerView() {
  const scheduler = await loadScheduler();
  setScheduler(state, scheduler);
}

async function loadMemoryView() {
  const [memory, processes] = await Promise.all([loadMemory(), loadProcesses()]);
  setMemory(state, memory);
  setProcesses(state, processes);
  if (state.selectedPid == null && memory?.largest_process?.pid != null) {
    setSelectedPid(state, memory.largest_process.pid);
  }
  await refreshSelectedProcess();
}

async function loadEventsView() {
  const events = await loadEvents();
  setEvents(state, events);
}

async function refreshRouteData(route) {
  const requestId = ++routeRequestId;
  const jobs = {
    home: loadHome,
    processes: loadProcessesView,
    scheduler: loadSchedulerView,
    memory: loadMemoryView,
    events: loadEventsView,
  };
  await (jobs[route] || loadProcessesView)();
  if (requestId !== routeRequestId) return;
  renderCurrentRoute();
}

function restartRouteTimer(route) {
  if (routeTimer) clearInterval(routeTimer);
  refreshRouteData(route);
  routeTimer = window.setInterval(() => refreshRouteData(route), routeIntervals[route] || 1000);
}

function handleRouteChange(route) {
  setRoute(state, route);
  resetShellScroll();
  renderShell();
  renderCurrentRoute();
  restartRouteTimer(route);
}

window.setInterval(refreshShell, 1000);
resetShellScroll(true);
refreshShell();
router = createRouter(handleRouteChange);
router.start();
