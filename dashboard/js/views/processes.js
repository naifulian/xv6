import { bindRouteButtons, escapeHtml, formatNumber, formatPercent, stateClass } from "../utils.js";

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
  const notes = detail?.notes || [
    "当前详情优先解释调度字段与地址空间口径。",
    "sz 表示逻辑地址空间规模，不等价于 RSS。",
  ];

  root.innerHTML = `
    <section class="page-shell">
      <article class="table-shell">
        <header>
          <div>
            <p class="section-kicker">Processes</p>
            <h2>进程主表</h2>
            <p class="table-caption">主表只回答“现在有哪些进程在运行”，右侧详情负责解释为什么值得看。</p>
          </div>
          <button class="back-link" data-route="home">返回主页</button>
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
                  : '<tr><td colspan="10" class="empty-note">暂无进程快照。先运行统一启动脚本并让 guest 开始输出 telemetry。</td></tr>'
              }
            </tbody>
          </table>
        </div>
      </article>

      <div class="sidebar-stack">
        <article class="detail-card">
          <header>
            <div>
              <p class="section-kicker">Focus</p>
              <h2>选中进程</h2>
            </div>
          </header>
          ${
            detail
              ? `
                <div class="focus-hero">
                  <p>PID ${escapeHtml(detail.pid)}</p>
                  <h3>${escapeHtml(detail.name || "unknown")}</h3>
                  <p>${escapeHtml(detail.state || "UNKNOWN")} · ${escapeHtml(detail.sched || "UNKNOWN")}</p>
                </div>
                <div class="detail-grid" style="margin-top: 16px;">
                  <div class="info-row"><span>总时长</span><strong>${escapeHtml(formatNumber(totalTime))}</strong></div>
                  <div class="info-row"><span>等待占比</span><strong>${escapeHtml(formatPercent(detail.wait_share))}</strong></div>
                  <div class="info-row"><span>运行占比</span><strong>${escapeHtml(formatPercent(detail.run_share))}</strong></div>
                  <div class="info-row"><span>睡眠占比</span><strong>${escapeHtml(formatPercent(detail.sleep_share))}</strong></div>
                </div>
                <div class="sidebar-stack" style="margin-top: 16px;">
                  <div>
                    <p class="section-kicker">调度解释</p>
                    <div class="note-stack">${schedulerRows.map((row) => `<div class="info-row"><span>${escapeHtml(row.label)}</span><strong>${escapeHtml(formatNumber(row.value))}</strong></div>`).join("")}</div>
                  </div>
                  <div>
                    <p class="section-kicker">地址空间摘要</p>
                    <div class="note-stack">${memoryRows.map((row) => `<div class="info-row"><span>${escapeHtml(row.label)}</span><strong>${escapeHtml(formatNumber(row.value))}</strong></div>`).join("")}</div>
                  </div>
                </div>
                <div style="margin-top: 16px;">
                  <p class="section-kicker">VMA 列表</p>
                  ${
                    vmas.length
                      ? `<div class="table-wrap" style="margin-top: 10px;"><table><thead><tr><th>Slot</th><th>Range</th><th>Pages</th><th>Prot</th><th>Flags</th></tr></thead><tbody>${vmas.map((row) => `<tr><td>${escapeHtml(formatNumber(row.slot))}</td><td>${escapeHtml(row.range_label)}</td><td>${escapeHtml(formatNumber(row.pages))}</td><td>${escapeHtml(row.prot_label)}</td><td>${escapeHtml(row.flags_label)}</td></tr>`).join("")}</tbody></table></div>`
                      : '<p class="empty-note" style="margin-top: 10px;">当前选中进程没有可展示的 VMA 快照。</p>'
                  }
                </div>
              `
              : '<p class="empty-note">当前没有可展示的进程详情。</p>'
          }
        </article>

        <article class="note-card">
          <p class="section-kicker">机制说明</p>
          <h3>为什么这个页面必须拆出来</h3>
          <ul class="note-list">
            <li>主表负责列出现状，详情区负责解释时间分解与地址空间。</li>
            <li>这样讲进程时不会被调度图和事件流打断叙事。</li>
            <li>返回主页后，可以继续按论文主线切到别的子页面。</li>
          </ul>
        </article>

        <article class="command-card">
          <p class="section-kicker">建议演示命令</p>
          <h3>适合在这一页触发的 guest 操作</h3>
          <ul class="command-list">
            <li>`ps`：让主表稳定刷新，解释 RUNNING / RUNNABLE 差别。</li>
            <li>`scheduler_test`：制造多个竞争进程，观察时间分解变化。</li>
            <li>`mmap_test`：触发 VMA 数和映射字节变化。</li>
          </ul>
          <ul class="note-list">
            ${notes.map((note) => `<li>${escapeHtml(note)}</li>`).join("")}
          </ul>
        </article>
      </div>
    </section>
  `;

  bindRouteButtons(root, actions);
  root.querySelectorAll("[data-pid]").forEach((row) => {
    row.addEventListener("click", () => actions.selectProcess(Number(row.dataset.pid)));
  });
}
