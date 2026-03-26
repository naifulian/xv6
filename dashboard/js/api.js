async function fetchJson(path) {
  try {
    const response = await fetch(path, { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    return await response.json();
  } catch (_) {
    return null;
  }
}

export const loadMeta = () => fetchJson("/api/meta");
export const loadSummary = () => fetchJson("/api/summary");
export const loadProcesses = () => fetchJson("/api/processes");
export const loadProcessDetail = (pid) => fetchJson(`/api/processes/${pid}`);
export const loadScheduler = () => fetchJson("/api/scheduler");
export const loadMemory = () => fetchJson("/api/memory");
export const loadEvents = () => fetchJson("/api/events");
