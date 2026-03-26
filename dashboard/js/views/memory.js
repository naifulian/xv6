import { bindRouteButtons, escapeHtml, formatNumber, formatPercent } from "../utils.js";

export function renderMemory(root, state, actions) {
  const memory = state.memory;
  const totals = memory.totals || {};
  const focus = state.selectedDetail || memory.largest_process || null;
  const notes = memory.notes || [];
  const vmas = focus?.vmas || [];

  root.innerHTML = `
    <section class="view-stack">
      <section class="page-hero">
        <article class="hero-card">
          <header>
            <div>
              <p class="section-kicker">Memory</p>
              <h2>系统页级口径 + 进程地址空间</h2>
            </div>
            <button class="back-link" data-route="home">返回主页</button>
          </header>
          <div class="summary-grid">
            <div class="summary-card"><p class="section-kicker">System</p><h3>Total Pages</h3><div class="info-row"><span>页数</span><strong>${escapeHtml(formatNumber(totals.total_pages))}</strong></div></div>
            <div class="summary-card"><p class="section-kicker">System</p><h3>Free Pages</h3><div class="info-row"><span>空闲</span><strong>${escapeHtml(formatNumber(totals.free_pages))}</strong></div></div>
            <div class="summary-card"><p class="section-kicker">Pressure</p><h3>Used Pages</h3><div class="info-row"><span>使用率</span><strong>${escapeHtml(formatPercent(totals.pressure_percent))}</strong></div></div>
            <div class="summary-card"><p class="section-kicker">Faults</p><h3>COW / Lazy</h3><div class="info-row"><span>累计</span><strong>${escapeHtml(`${formatNumber(totals.cow_faults)} / ${formatNumber(totals.lazy_allocs)}`)}</strong></div></div>
          </div>
        </article>
        <article class="command-card">
          <p class="section-kicker">观测口径</p>
          <h2>这页不伪装成 Linux 内存监控</h2>
          <ul class="note-list">
            <li>sz 只表示逻辑地址空间规模。</li>
            <li>heap_end 表示当前堆顶，VMA 代表 mmap 视图。</li>
            <li>本页重点是页数、fault 和地址空间语义，不是 RSS/缓存命中率。</li>
          </ul>
        </article>
      </section>

      <section class="page-shell">
        <article class="detail-card">
          <header>
            <div>
              <p class="section-kicker">Focus Process</p>
              <h2>${escapeHtml(focus ? `PID ${focus.pid} · ${focus.name || "unknown"}` : "等待进程详情")}</h2>
              <p class="table-caption">默认优先看你在进程页选中的进程；如果还没选中过，则回退到本次快照里 sz 最大的进程。</p>
            </div>
          </header>
          ${
            focus
              ? `
                <div class="detail-grid">
                  <div class="info-row"><span>逻辑地址空间 sz</span><strong>${escapeHtml(formatNumber(focus.sz))}</strong></div>
                  <div class="info-row"><span>heap_end</span><strong>${escapeHtml(formatNumber(focus.heap_end))}</strong></div>
                  <div class="info-row"><span>VMA 数</span><strong>${escapeHtml(formatNumber(focus.vma_count))}</strong></div>
                  <div class="info-row"><span>mmap regions</span><strong>${escapeHtml(formatNumber(focus.mmap_regions))}</strong></div>
                  <div class="info-row"><span>mmap bytes</span><strong>${escapeHtml(formatNumber(focus.mmap_bytes))}</strong></div>
                  <div class="info-row"><span>系统 free pages</span><strong>${escapeHtml(formatNumber(state.summary.system?.free_pages))}</strong></div>
                </div>
                <div style="margin-top: 16px;">
                  <p class="section-kicker">VMA 细节</p>
                  ${
                    vmas.length
                      ? `<div class="table-wrap" style="margin-top: 10px;"><table><thead><tr><th>Slot</th><th>Range</th><th>Pages</th><th>Prot</th><th>Flags</th></tr></thead><tbody>${vmas.map((row) => `<tr><td>${escapeHtml(formatNumber(row.slot))}</td><td>${escapeHtml(row.range_label)}</td><td>${escapeHtml(formatNumber(row.pages))}</td><td>${escapeHtml(row.prot_label)}</td><td>${escapeHtml(row.flags_label)}</td></tr>`).join("")}</tbody></table></div>`
                      : '<p class="empty-note" style="margin-top: 10px;">当前没有可展示的 VMA 行。</p>'
                  }
                </div>
              `
              : '<p class="empty-note">当前没有可用的进程地址空间详情。</p>'
          }
        </article>

        <div class="sidebar-stack">
          <article class="note-card">
            <p class="section-kicker">系统摘要</p>
            <h3>聚焦真正有论文主线价值的指标</h3>
            <div class="note-stack" style="margin-top: 14px;">
              <div class="info-row"><span>total VMA</span><strong>${escapeHtml(formatNumber(totals.total_vmas))}</strong></div>
              <div class="info-row"><span>mmap regions</span><strong>${escapeHtml(formatNumber(totals.total_mmap_regions))}</strong></div>
              <div class="info-row"><span>mmap bytes</span><strong>${escapeHtml(formatNumber(totals.total_mmap_bytes))}</strong></div>
              <div class="info-row"><span>cow copy pages</span><strong>${escapeHtml(formatNumber(totals.cow_copy_pages))}</strong></div>
            </div>
          </article>

          <article class="command-card">
            <p class="section-kicker">建议演示命令</p>
            <h3>这页适合讲地址空间语义</h3>
            <ul class="command-list">
              <li><code>mmap_test</code>：直接展示 VMA 数与映射范围变化。</li>
              <li><code>mmap_lazy_test</code>：观察 lazy fault 与按需分配行为。</li>
              <li><code>memstat</code>：和本页口径对照 total/free pages 变化。</li>
            </ul>
            <ul class="note-list">
              ${notes.map((note) => `<li>${escapeHtml(note)}</li>`).join("")}
            </ul>
          </article>
        </div>
      </section>
    </section>
  `;

  bindRouteButtons(root, actions);
}
