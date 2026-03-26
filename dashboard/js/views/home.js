import { escapeHtml, formatNumber, formatPercent, stateClass } from "../utils.js";

export function renderHome(root, state, actions) {
  const summary = state.summary;
  const system = summary.system || {};
  const memory = state.memory?.totals || {};
  const processes = (state.processes || []).slice(0, 8);
  const events = state.events.slice().reverse().slice(0, 8);

  root.innerHTML = `
    <section class="page-stack">
      <section class="metric-grid">
        <article class="metric-card">
          <div class="metric-label">当前策略</div>
          <div class="metric-value">${escapeHtml(formatNumber(summary.policy?.label || summary.policy?.name || system.policy))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">进程数</div>
          <div class="metric-value">${escapeHtml(formatNumber(system.nr_total))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">空闲页</div>
          <div class="metric-value">${escapeHtml(formatNumber(memory.free_pages))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">内存压力</div>
          <div class="metric-value">${escapeHtml(formatPercent(memory.pressure_percent))}</div>
        </article>
      </section>

      <section class="content-grid overview-layout">
        <article class="panel section-panel">
          <header class="section-header">
            <div>
              <div class="metric-label">进程</div>
              <h3>当前快照</h3>
              <p class="table-caption">点击行可直接进入进程页。</p>
            </div>
          </header>
          <div class="table-wrap">
            <table>
              <thead>
                <tr>
                  <th>PID</th>
                  <th>Name</th>
                  <th>State</th>
                  <th>Sched</th>
                  <th>rutime</th>
                  <th>sz</th>
                </tr>
              </thead>
              <tbody>
                ${
                  processes.length
                    ? processes
                        .map(
                          (proc) => `
                            <tr data-pid="${proc.pid}" class="link-row">
                              <td>${escapeHtml(formatNumber(proc.pid))}</td>
                              <td>${escapeHtml(proc.name || "unknown")}</td>
                              <td><span class="${stateClass(proc.state)}">${escapeHtml(proc.state || "UNKNOWN")}</span></td>
                              <td>${escapeHtml(proc.sched || "UNKNOWN")}</td>
                              <td>${escapeHtml(formatNumber(proc.rutime))}</td>
                              <td>${escapeHtml(formatNumber(proc.sz))}</td>
                            </tr>
                          `,
                        )
                        .join("")
                    : '<tr><td colspan="6" class="empty-note">暂无进程快照。</td></tr>'
                }
              </tbody>
            </table>
          </div>
        </article>

        <div class="side-stack">
          <article class="panel section-panel">
            <header class="section-header">
              <div>
                <div class="metric-label">系统</div>
                <h3>摘要</h3>
              </div>
            </header>
            <div class="list-stack">
              <div class="info-row"><span>运行队列</span><strong>${escapeHtml(formatNumber(system.runqueue))}</strong></div>
              <div class="info-row"><span>上下文切换</span><strong>${escapeHtml(formatNumber(system.context_switches))}</strong></div>
              <div class="info-row"><span>RUNNING</span><strong>${escapeHtml(formatNumber(system.nr_running))}</strong></div>
              <div class="info-row"><span>SLEEPING</span><strong>${escapeHtml(formatNumber(system.nr_sleeping))}</strong></div>
              <div class="info-row"><span>ZOMBIE</span><strong>${escapeHtml(formatNumber(system.nr_zombie))}</strong></div>
            </div>
          </article>

          <article class="panel section-panel">
            <header class="section-header">
              <div>
                <div class="metric-label">事件</div>
                <h3>最近变化</h3>
              </div>
            </header>
            ${
              events.length
                ? `<ul class="timeline-list">${events
                    .map(
                      (event) => `
                        <li class="timeline-item">
                          <div class="timeline-top">
                            <strong>${escapeHtml(event.title || "未命名事件")}</strong>
                            <span class="inline-chip">seq ${escapeHtml(formatNumber(event.seq))}</span>
                          </div>
                          <div class="timeline-meta">${escapeHtml(event.kind || "unknown")}</div>
                          <p>${escapeHtml(event.detail || "")}</p>
                        </li>
                      `,
                    )
                    .join("")}</ul>`
                : '<div class="empty-note">暂无事件。</div>'
            }
          </article>
        </div>
      </section>
    </section>
  `;

  root.querySelectorAll("[data-pid]").forEach((row) => {
    row.addEventListener("click", () => {
      const pid = Number(row.dataset.pid);
      actions.navigate("processes");
      actions.selectProcess(pid);
    });
  });
}
