import { escapeHtml, formatNumber, formatPercent, stateClass } from "../utils.js";

export function renderProcesses(root, state, actions) {
  const processes = state.processes || [];
  const detail = state.selectedDetail || processes.find((proc) => proc.pid === state.selectedPid) || null;
  const totalTime = detail ? Number(detail.rutime || 0) + Number(detail.retime || 0) + Number(detail.stime || 0) : 0;
  const vmas = detail?.vmas || [];
  const schedulerRows = detail?.scheduler_rows || [
    { label: "当前策略", value: detail?.sched || state.summary.policy?.label || "UNKNOWN" },
    { label: "priority", value: detail?.priority },
    { label: "tickets", value: detail?.tickets },
    { label: "运行队列", value: state.summary.system?.runqueue },
  ];
  const memoryRows = detail?.memory_rows || [
    { label: "逻辑地址空间 sz", value: detail?.sz },
    { label: "heap_end", value: detail?.heap_end },
    { label: "VMA 数", value: detail?.vma_count },
    { label: "mmap bytes", value: detail?.mmap_bytes },
  ];
  const system = state.summary.system || {};

  root.innerHTML = `
    <section class="page-stack">
      <section class="metric-grid">
        <article class="metric-card">
          <div class="metric-label">总进程</div>
          <div class="metric-value">${escapeHtml(formatNumber(system.nr_total))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">Running</div>
          <div class="metric-value">${escapeHtml(formatNumber(system.nr_running))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">Sleeping</div>
          <div class="metric-value">${escapeHtml(formatNumber(system.nr_sleeping))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">Zombie</div>
          <div class="metric-value">${escapeHtml(formatNumber(system.nr_zombie))}</div>
        </article>
      </section>

      <section class="content-grid processes-layout">
        <article class="panel section-panel">
          <header class="section-header">
            <div>
              <div class="metric-label">进程</div>
              <h3>进程列表</h3>
              <p class="table-caption">点击行查看右侧详情。</p>
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
                  <th>Priority</th>
                  <th>Tickets</th>
                  <th>rutime</th>
                  <th>retime</th>
                  <th>stime</th>
                  <th>sz</th>
                </tr>
              </thead>
              <tbody>
                ${
                  processes.length
                    ? processes
                        .map(
                          (proc) => `
                            <tr data-pid="${proc.pid}" class="${proc.pid === state.selectedPid ? "selected" : ""}">
                              <td>${escapeHtml(formatNumber(proc.pid))}</td>
                              <td>${escapeHtml(proc.name || "unknown")}</td>
                              <td><span class="${stateClass(proc.state)}">${escapeHtml(proc.state || "UNKNOWN")}</span></td>
                              <td>${escapeHtml(proc.sched || "UNKNOWN")}</td>
                              <td>${escapeHtml(formatNumber(proc.priority))}</td>
                              <td>${escapeHtml(formatNumber(proc.tickets))}</td>
                              <td>${escapeHtml(formatNumber(proc.rutime))}</td>
                              <td>${escapeHtml(formatNumber(proc.retime))}</td>
                              <td>${escapeHtml(formatNumber(proc.stime))}</td>
                              <td>${escapeHtml(formatNumber(proc.sz))}</td>
                            </tr>
                          `,
                        )
                        .join("")
                    : '<tr><td colspan="10" class="empty-note">暂无进程快照。</td></tr>'
                }
              </tbody>
            </table>
          </div>
        </article>

        <div class="side-stack">
          <article class="panel section-panel">
            <header class="section-header">
              <div>
                <div class="metric-label">详情</div>
                <h3>当前进程</h3>
              </div>
            </header>
            ${
              detail
                ? `
                  <div class="focus-card">
                    <div class="focus-title">
                      <div>
                        <div class="metric-label">PID ${escapeHtml(detail.pid)}</div>
                        <h4>${escapeHtml(detail.name || "unknown")}</h4>
                      </div>
                      <span class="${stateClass(detail.state)}">${escapeHtml(detail.state || "UNKNOWN")}</span>
                    </div>
                    <p class="subtle-text">${escapeHtml(detail.sched || "UNKNOWN")}</p>
                  </div>

                  <div class="info-grid">
                    <div class="info-row"><span>总时长</span><strong>${escapeHtml(formatNumber(totalTime))}</strong></div>
                    <div class="info-row"><span>等待占比</span><strong>${escapeHtml(formatPercent(detail.wait_share))}</strong></div>
                    <div class="info-row"><span>运行占比</span><strong>${escapeHtml(formatPercent(detail.run_share))}</strong></div>
                    <div class="info-row"><span>睡眠占比</span><strong>${escapeHtml(formatPercent(detail.sleep_share))}</strong></div>
                  </div>

                  <div class="list-stack">
                    ${schedulerRows
                      .map(
                        (row) => `<div class="info-row"><span>${escapeHtml(row.label)}</span><strong>${escapeHtml(formatNumber(row.value))}</strong></div>`,
                      )
                      .join("")}
                  </div>

                  <div class="list-stack">
                    ${memoryRows
                      .map(
                        (row) => `<div class="info-row"><span>${escapeHtml(row.label)}</span><strong>${escapeHtml(formatNumber(row.value))}</strong></div>`,
                      )
                      .join("")}
                  </div>

                  <div>
                    <div class="section-header">
                      <div>
                        <div class="metric-label">VMA</div>
                        <h3>映射</h3>
                      </div>
                    </div>
                    ${
                      vmas.length
                        ? `<div class="table-wrap"><table><thead><tr><th>Slot</th><th>Range</th><th>Pages</th><th>Prot</th><th>Flags</th></tr></thead><tbody>${vmas
                            .map(
                              (row) => `<tr><td>${escapeHtml(formatNumber(row.slot))}</td><td>${escapeHtml(row.range_label)}</td><td>${escapeHtml(formatNumber(row.pages))}</td><td>${escapeHtml(row.prot_label)}</td><td>${escapeHtml(row.flags_label)}</td></tr>`,
                            )
                            .join("")}</tbody></table></div>`
                        : '<div class="empty-note">当前没有 VMA 快照。</div>'
                    }
                  </div>
                `
                : '<div class="empty-note">当前没有可展示的进程详情。</div>'
            }
          </article>

          <article class="panel section-panel">
            <header class="section-header">
              <div>
                <div class="metric-label">系统</div>
                <h3>当前摘要</h3>
              </div>
            </header>
            <div class="list-stack">
              <div class="info-row"><span>当前策略</span><strong>${escapeHtml(formatNumber(state.summary.policy?.label || state.summary.policy?.name || system.policy))}</strong></div>
              <div class="info-row"><span>运行队列</span><strong>${escapeHtml(formatNumber(system.runqueue))}</strong></div>
              <div class="info-row"><span>上下文切换</span><strong>${escapeHtml(formatNumber(system.context_switches))}</strong></div>
              <div class="info-row"><span>空闲页</span><strong>${escapeHtml(formatNumber(system.free_pages))}</strong></div>
            </div>
          </article>
        </div>
      </section>
    </section>
  `;

  root.querySelectorAll("[data-pid]").forEach((row) => {
    row.addEventListener("click", () => actions.selectProcess(Number(row.dataset.pid)));
  });
}
