import { bindRouteButtons, escapeHtml, formatNumber } from "../utils.js";

export function renderScheduler(root, state, actions) {
  const scheduler = state.scheduler;
  const cards = scheduler.cards || [];
  const leaders = scheduler.leaders || [];
  const breakdown = scheduler.state_breakdown || [];
  const notes = scheduler.notes || [];
  const spotlight = scheduler.spotlight || { title: "等待遥测输入", detail: "暂无摘要。" };

  root.innerHTML = `
    <section class="view-stack">
      <section class="page-hero">
        <article class="hero-card">
          <header>
            <div>
              <p class="section-kicker">Scheduler</p>
              <h2>调度观察页</h2>
            </div>
            <button class="back-link" data-route="home">返回主页</button>
          </header>
          <p>${escapeHtml(spotlight.detail)}</p>
          <div class="card-grid" style="margin-top: 16px;">
            ${cards
              .map(
                (card) => `
                  <article class="data-card">
                    <p class="section-kicker">Policy Card</p>
                    <h3>${escapeHtml(card.label)}</h3>
                    <div class="info-row"><span>当前值</span><strong>${escapeHtml(formatNumber(card.value))}</strong></div>
                    <p class="helper-copy" style="margin-top: 10px;">${escapeHtml(card.description)}</p>
                  </article>
                `,
              )
              .join("")}
          </div>
        </article>
        <article class="summary-card">
          <p class="section-kicker">观察焦点</p>
          <h2>${escapeHtml(spotlight.title)}</h2>
          <ul class="note-list" style="margin-top: 12px;">
            <li>当前策略：${escapeHtml(formatNumber(scheduler.policy?.label || scheduler.policy?.name))}</li>
            <li>主字段：${escapeHtml(formatNumber(scheduler.metric_label || scheduler.metric))}</li>
            <li>当前目标不是做“所有策略都一样的图”，而是讲清每种策略真正该看的字段。</li>
          </ul>
        </article>
      </section>

      <section class="page-shell">
        <article class="timeline-card">
          <header>
            <div>
              <p class="section-kicker">Leader Board</p>
              <h2>热点进程</h2>
              <p class="table-caption">按当前策略的主观察字段排序，而不是简单复用进程主表。</p>
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
                            <p class="section-kicker">PID ${escapeHtml(leader.pid)}</p>
                            <h3>${escapeHtml(leader.name)}</h3>
                          </div>
                          <span class="inline-chip">${escapeHtml(leader.state)}</span>
                        </div>
                        <div class="info-row" style="margin-top: 12px;"><span>${escapeHtml(leader.metric_label)}</span><strong>${escapeHtml(formatNumber(leader.value))}</strong></div>
                        <p style="margin-top: 12px;">${escapeHtml(leader.detail)}</p>
                      </article>
                    `,
                  )
                  .join("")}</div>`
              : '<p class="empty-note">暂无策略热点进程。</p>'
          }
        </article>

        <div class="sidebar-stack">
          <article class="note-card">
            <p class="section-kicker">状态分布</p>
            <h3>当前快照里有哪些进程状态</h3>
            <div class="note-stack" style="margin-top: 14px;">
              ${
                breakdown.length
                  ? breakdown.map((item) => `<div class="info-row"><span>${escapeHtml(item.label)}</span><strong>${escapeHtml(formatNumber(item.value))}</strong></div>`).join("")
                  : '<p class="empty-note">暂无状态分布。</p>'
              }
            </div>
          </article>

          <article class="command-card">
            <p class="section-kicker">建议演示命令</p>
            <h3>这一页适合讲“为什么策略不同”</h3>
            <ul class="command-list">
              <li><code>scheduler_test</code>：制造不同负载，解释等待时间与运行时间的差异。</li>
              <li><code>test_mlfq</code> / <code>test_cfs</code>：对照不同策略下的热点进程排序。</li>
              <li><code>chsched</code>：切换策略后回到本页看重点字段是否随策略变化。</li>
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
