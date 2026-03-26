import { escapeHtml, formatNumber, formatPercent } from "../utils.js";

export function renderMemory(root, state) {
  const memory = state.memory;
  const totals = memory.totals || {};
  const focus = state.selectedDetail || memory.largest_process || null;
  const vmas = focus?.vmas || [];

  root.innerHTML = `
    <section class="page-stack">
      <section class="metric-grid">
        <article class="metric-card">
          <div class="metric-label">总页数</div>
          <div class="metric-value">${escapeHtml(formatNumber(totals.total_pages))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">空闲页</div>
          <div class="metric-value">${escapeHtml(formatNumber(totals.free_pages))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">已用页</div>
          <div class="metric-value">${escapeHtml(formatNumber(totals.used_pages))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">压力</div>
          <div class="metric-value">${escapeHtml(formatPercent(totals.pressure_percent))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">COW Fault</div>
          <div class="metric-value">${escapeHtml(formatNumber(totals.cow_faults))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">Lazy Alloc</div>
          <div class="metric-value">${escapeHtml(formatNumber(totals.lazy_allocs))}</div>
        </article>
      </section>

      <section class="content-grid memory-layout">
        <article class="panel section-panel">
          <header class="section-header">
            <div>
              <div class="metric-label">进程地址空间</div>
              <h3>${escapeHtml(focus ? `PID ${focus.pid} · ${focus.name || "unknown"}` : "当前焦点")}</h3>
            </div>
          </header>
          ${
            focus
              ? `
                <div class="info-grid">
                  <div class="info-row"><span>逻辑地址空间 sz</span><strong>${escapeHtml(formatNumber(focus.sz))}</strong></div>
                  <div class="info-row"><span>heap_end</span><strong>${escapeHtml(formatNumber(focus.heap_end))}</strong></div>
                  <div class="info-row"><span>VMA 数</span><strong>${escapeHtml(formatNumber(focus.vma_count))}</strong></div>
                  <div class="info-row"><span>mmap regions</span><strong>${escapeHtml(formatNumber(focus.mmap_regions))}</strong></div>
                  <div class="info-row"><span>mmap bytes</span><strong>${escapeHtml(formatNumber(focus.mmap_bytes))}</strong></div>
                  <div class="info-row"><span>系统空闲页</span><strong>${escapeHtml(formatNumber(state.summary.system?.free_pages))}</strong></div>
                </div>
                <div>
                  <div class="section-header">
                    <div>
                      <div class="metric-label">VMA</div>
                      <h3>映射列表</h3>
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
              : '<div class="empty-note">当前没有可展示的进程地址空间详情。</div>'
          }
        </article>

        <div class="side-stack">
          <article class="panel section-panel">
            <header class="section-header">
              <div>
                <div class="metric-label">系统</div>
                <h3>汇总</h3>
              </div>
            </header>
            <div class="list-stack">
              <div class="info-row"><span>total VMA</span><strong>${escapeHtml(formatNumber(totals.total_vmas))}</strong></div>
              <div class="info-row"><span>mmap regions</span><strong>${escapeHtml(formatNumber(totals.total_mmap_regions))}</strong></div>
              <div class="info-row"><span>mmap bytes</span><strong>${escapeHtml(formatNumber(totals.total_mmap_bytes))}</strong></div>
              <div class="info-row"><span>cow copy pages</span><strong>${escapeHtml(formatNumber(totals.cow_copy_pages))}</strong></div>
            </div>
          </article>
        </div>
      </section>
    </section>
  `;
}
