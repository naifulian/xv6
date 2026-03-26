import { escapeHtml, formatNumber } from "../utils.js";

export function renderScheduler(root, state) {
  const scheduler = state.scheduler;
  const cards = scheduler.cards || [];
  const leaders = scheduler.leaders || [];
  const breakdown = scheduler.state_breakdown || [];
  const system = state.summary.system || {};

  root.innerHTML = `
    <section class="page-stack">
      <section class="metric-grid">
        <article class="metric-card">
          <div class="metric-label">当前策略</div>
          <div class="metric-value">${escapeHtml(formatNumber(scheduler.policy?.label || scheduler.policy?.name))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">关键字段</div>
          <div class="metric-value">${escapeHtml(formatNumber(scheduler.metric_label || scheduler.metric))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">运行队列</div>
          <div class="metric-value">${escapeHtml(formatNumber(system.runqueue))}</div>
        </article>
        <article class="metric-card">
          <div class="metric-label">上下文切换</div>
          <div class="metric-value">${escapeHtml(formatNumber(system.context_switches))}</div>
        </article>
      </section>

      <section class="content-grid scheduler-layout">
        <article class="panel section-panel">
          <header class="section-header">
            <div>
              <div class="metric-label">热点进程</div>
              <h3>排序结果</h3>
            </div>
          </header>
          ${
            leaders.length
              ? `<div class="leader-list">${leaders
                  .map(
                    (leader) => `
                      <article class="leader-card">
                        <div class="leader-top">
                          <div>
                            <div class="metric-label">PID ${escapeHtml(leader.pid)}</div>
                            <h4>${escapeHtml(leader.name || "unknown")}</h4>
                          </div>
                          <span class="inline-chip">${escapeHtml(leader.state || "UNKNOWN")}</span>
                        </div>
                        <div class="info-row"><span>${escapeHtml(leader.metric_label || scheduler.metric_label || scheduler.metric)}</span><strong>${escapeHtml(formatNumber(leader.value))}</strong></div>
                        ${leader.detail ? `<p class="subtle-text">${escapeHtml(leader.detail)}</p>` : ""}
                      </article>
                    `,
                  )
                  .join("")}</div>`
              : '<div class="empty-note">暂无策略热点进程。</div>'
          }
        </article>

        <div class="side-stack">
          <article class="panel section-panel">
            <header class="section-header">
              <div>
                <div class="metric-label">策略指标</div>
                <h3>当前卡片</h3>
              </div>
            </header>
            ${
              cards.length
                ? `<div class="list-stack">${cards
                    .map(
                      (card) => `
                        <div class="summary-box">
                          <div class="metric-label">${escapeHtml(card.label)}</div>
                          <div class="metric-value">${escapeHtml(formatNumber(card.value))}</div>
                          ${card.description ? `<p class="subtle-text">${escapeHtml(card.description)}</p>` : ""}
                        </div>
                      `,
                    )
                    .join("")}</div>`
                : '<div class="empty-note">暂无策略指标。</div>'
            }
          </article>

          <article class="panel section-panel">
            <header class="section-header">
              <div>
                <div class="metric-label">状态</div>
                <h3>分布</h3>
              </div>
            </header>
            ${
              breakdown.length
                ? `<div class="list-stack">${breakdown
                    .map(
                      (item) => `<div class="info-row"><span>${escapeHtml(item.label)}</span><strong>${escapeHtml(formatNumber(item.value))}</strong></div>`,
                    )
                    .join("")}</div>`
                : '<div class="empty-note">暂无状态分布。</div>'
            }
          </article>
        </div>
      </section>
    </section>
  `;
}
