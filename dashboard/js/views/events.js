import { escapeHtml, formatNumber } from "../utils.js";

export function renderEvents(root, state) {
  const events = state.events.slice().reverse();
  const latest = events[0] || null;
  const kinds = new Map();

  state.events.forEach((event) => {
    const kind = event.kind || "unknown";
    kinds.set(kind, (kinds.get(kind) || 0) + 1);
  });

  root.innerHTML = `
    <section class="page-stack">
      <section class="metric-grid">
        <article class="metric-card">
          <div class="metric-label">最近事件数</div>
          <div class="metric-value">${escapeHtml(formatNumber(state.events.length))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">最新序号</div>
          <div class="metric-value">${escapeHtml(formatNumber(latest?.seq))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">最新标题</div>
          <div class="metric-value">${escapeHtml(latest?.title || "暂无")}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">事件类型</div>
          <div class="metric-value">${escapeHtml(formatNumber(kinds.size))}</div>
        </article>
      </section>

      <section class="content-grid events-layout">
        <article class="panel section-panel">
          <header class="section-header">
            <div>
              <div class="metric-label">时间线</div>
              <h3>最近事件</h3>
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
              : '<div class="empty-note">暂无事件差分结果。</div>'
          }
        </article>

        <div class="side-stack">
          <article class="panel section-panel">
            <header class="section-header">
              <div>
                <div class="metric-label">分布</div>
                <h3>事件类型</h3>
              </div>
            </header>
            ${
              kinds.size
                ? `<div class="list-stack">${[...kinds.entries()]
                    .map(
                      ([kind, count]) => `<div class="info-row"><span>${escapeHtml(kind)}</span><strong>${escapeHtml(formatNumber(count))}</strong></div>`,
                    )
                    .join("")}</div>`
                : '<div class="empty-note">暂无事件分布。</div>'
            }
          </article>

          <article class="panel section-panel">
            <header class="section-header">
              <div>
                <div class="metric-label">最新事件</div>
                <h3>详情</h3>
              </div>
            </header>
            ${
              latest
                ? `<div class="focus-card"><div class="metric-label">${escapeHtml(latest.kind || "unknown")}</div><h4>${escapeHtml(latest.title || "未命名事件")}</h4><p>${escapeHtml(latest.detail || "")}</p></div>`
                : '<div class="empty-note">暂无最新事件。</div>'
            }
          </article>
        </div>
      </section>
    </section>
  `;
}
