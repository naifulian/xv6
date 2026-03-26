import { bindRouteButtons, escapeHtml, formatNumber } from "../utils.js";

export function renderEvents(root, state, actions) {
  const events = state.events.slice().reverse();
  const latest = events[0] || null;
  const kinds = new Map();
  state.events.forEach((event) => {
    const kind = event.kind || "unknown";
    kinds.set(kind, (kinds.get(kind) || 0) + 1);
  });

  root.innerHTML = `
    <section class="view-stack">
      <section class="page-hero">
        <article class="hero-card">
          <header>
            <div>
              <p class="section-kicker">Events</p>
              <h2>事件时间线</h2>
            </div>
            <button class="back-link" data-route="home">返回主页</button>
          </header>
          <p>事件页把短时变化从主表中拆出来，专门展示 fork、exit、state change、mmap 和 fault 增长。</p>
          <div class="summary-grid" style="margin-top: 16px;">
            <div class="summary-card"><p class="section-kicker">Count</p><h3>最近事件数</h3><div class="info-row"><span>条目</span><strong>${escapeHtml(formatNumber(state.events.length))}</strong></div></div>
            <div class="summary-card"><p class="section-kicker">Latest</p><h3>最新标题</h3><div class="info-row"><span>事件</span><strong>${escapeHtml(latest?.title || "暂无")}</strong></div></div>
            <div class="summary-card"><p class="section-kicker">Seq</p><h3>最新序号</h3><div class="info-row"><span>seq</span><strong>${escapeHtml(formatNumber(latest?.seq))}</strong></div></div>
          </div>
        </article>
        <article class="command-card">
          <p class="section-kicker">观测边界</p>
          <h2>当前事件仍以快照差分为主</h2>
          <ul class="note-list">
            <li>这一版已经把事件从主表里独立出来，但 guest 侧原生 EVENT 帧还没有接入。</li>
            <li>因此它比单页拼盘更好讲，但对极短命命令仍有快照粒度上限。</li>
            <li>这部分已在审查文档里保留为下一阶段整改项。</li>
          </ul>
        </article>
      </section>

      <section class="page-shell">
        <article class="timeline-card">
          <header>
            <div>
              <p class="section-kicker">Timeline</p>
              <h2>最近事件</h2>
              <p class="table-caption">高频刷新层只承接事件，不再和进程主表混在同一屏里。</p>
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
              : '<p class="empty-note">暂无事件差分结果。</p>'
          }
        </article>

        <div class="sidebar-stack">
          <article class="note-card">
            <p class="section-kicker">事件分布</p>
            <h3>当前都在发生什么类型的变化</h3>
            <div class="note-stack" style="margin-top: 14px;">
              ${
                kinds.size
                  ? [...kinds.entries()].map(([kind, count]) => `<div class="info-row"><span>${escapeHtml(kind)}</span><strong>${escapeHtml(formatNumber(count))}</strong></div>`).join("")
                  : '<p class="empty-note">暂无事件分布。</p>'
              }
            </div>
          </article>

          <article class="command-card">
            <p class="section-kicker">建议演示命令</p>
            <h3>适合在这一页触发短时变化</h3>
            <ul class="command-list">
              <li><code>echo hello</code>、<code>ls</code>：观察短命命令在快照周期内是否可见。</li>
              <li><code>mmap_test</code>：触发 mmap / munmap 相关事件。</li>
              <li><code>scheduler_test</code>：让状态变化和 fault 增长更容易连续出现。</li>
            </ul>
          </article>
        </div>
      </section>
    </section>
  `;

  bindRouteButtons(root, actions);
}
