import { bindRouteButtons, escapeHtml, formatNumber, formatPercent, truncate } from "../utils.js";

const moduleCards = [
  {
    title: "模块一 · 机制设计",
    body: "统一调度框架、统一地址空间语义和 9 种策略，是后续观测页面的机制来源。",
    tag: "机制来源",
  },
  {
    title: "模块二 · 可信验证",
    body: "测试分类与关键回归保证这套机制不是只在 demo 页面上看起来成立。",
    tag: "可信边界",
  },
  {
    title: "模块三 · 正式实验",
    body: "实验结果给出性能与策略差异的书面证据，避免现场演示只靠主观感受。",
    tag: "正式证据",
  },
  {
    title: "模块四 · 现场展示",
    body: "当前页面把前面三个模块讲活，让评委在 live 运行态里看到论文闭环。",
    tag: "运行态入口",
  },
];

export function renderHome(root, state, actions) {
  const summary = state.summary;
  const system = summary.system || {};
  const policy = summary.policy || {};
  const memory = state.memory?.totals || {};
  const recentEvents = state.events.slice(-4).reverse();

  const entryCards = [
    {
      route: "processes",
      title: "进程管理",
      text: "从主表进入单个进程详情，讲解时间分解、地址空间和 VMA。",
      metrics: [
        ["总进程数", formatNumber(system.nr_total)],
        ["RUNNABLE/RUNNING", formatNumber(system.nr_running)],
        ["策略", formatNumber(system.policy)],
      ],
    },
    {
      route: "scheduler",
      title: "调度观察",
      text: "围绕当前策略展示教学重点，不再把调度信息埋在单页角落。",
      metrics: [
        ["策略标签", formatNumber(policy.label || policy.name)],
        ["运行队列", formatNumber(system.runqueue)],
        ["上下文切换", formatNumber(system.context_switches)],
      ],
    },
    {
      route: "memory",
      title: "内存管理",
      text: "先看系统页级压力，再看单进程 sz、heap_end 与 VMA 口径。",
      metrics: [
        ["Free Pages", formatNumber(memory.free_pages)],
        ["Pressure", formatPercent(memory.pressure_percent)],
        ["COW / Lazy", `${formatNumber(memory.cow_faults)} / ${formatNumber(memory.lazy_allocs)}`],
      ],
    },
    {
      route: "events",
      title: "事件时间线",
      text: "把进程短时变化从主表里拆出去，单独看 fork/exit/mmap/fault 热点。",
      metrics: [
        ["最近事件", formatNumber(state.events.length)],
        ["最新序号", formatNumber(summary.latest_seq)],
        ["最近标题", truncate(recentEvents[0]?.title || "暂无事件", 20)],
      ],
    },
  ];

  root.innerHTML = `
    <section class="view-stack">
      <section class="page-hero">
        <article class="hero-card dark">
          <p class="section-kicker">Defense Mode</p>
          <h2>先讲论文闭环，再进入 live monitor</h2>
          <p>模块四首页不再只是堆实时指标，而是先解释这套系统为什么值得展示、当前 live 页面能帮你证明什么。</p>
          <div class="card-metrics">
            <div class="metric-row"><span>当前策略</span><strong>${escapeHtml(formatNumber(policy.label || policy.name))}</strong></div>
            <div class="metric-row"><span>系统在线状态</span><strong>${escapeHtml(state.bridgeOnline && summary.online ? "Bridge + Telemetry 就绪" : state.bridgeOnline ? "Bridge 在线，等待 guest" : "Bridge 未连通")}</strong></div>
            <div class="metric-row"><span>推荐入口</span><strong>bash dashboard/scripts/start_monitor.sh</strong></div>
          </div>
        </article>
        <article class="summary-card">
          <p class="section-kicker">Teaching Mode</p>
          <h2>四个子页面稳定回答三个问题</h2>
          <ul class="note-list">
            <li>现在看到的是什么状态。</li>
            <li>它对应模块一里的哪个机制。</li>
            <li>为什么这个指标值得评委或学生关注。</li>
          </ul>
        </article>
      </section>

      <section class="module-grid">
        ${moduleCards
          .map(
            (item) => `
              <article class="module-card">
                <span class="inline-chip">${escapeHtml(item.tag)}</span>
                <h3>${escapeHtml(item.title)}</h3>
                <p>${escapeHtml(item.body)}</p>
              </article>
            `,
          )
          .join("")}
      </section>

      <section class="entry-grid">
        ${entryCards
          .map(
            (card) => `
              <article class="entry-card">
                <p class="section-kicker">Live Entry</p>
                <h3>${escapeHtml(card.title)}</h3>
                <p>${escapeHtml(card.text)}</p>
                <div class="card-metrics">
                  ${card.metrics
                    .map(
                      ([label, value]) => `<div class="metric-row"><span>${escapeHtml(label)}</span><strong>${escapeHtml(value)}</strong></div>`,
                    )
                    .join("")}
                </div>
                <button class="entry-action" data-route="${card.route}">进入 ${escapeHtml(card.title)}</button>
              </article>
            `,
          )
          .join("")}
      </section>

      <section class="note-grid">
        <article class="note-card">
          <p class="section-kicker">观测口径</p>
          <h3>页面必须诚实，不伪装成通用 Linux 监控器</h3>
          <ul class="note-list">
            <li>sz 只表示逻辑地址空间规模，不等价于 RSS。</li>
            <li>当前内存口径围绕页数、fault 与 VMA，而不是 Linux 全量指标。</li>
            <li>默认运行链已切到 telemetry side channel，双终端仅保留为 fallback/debug。</li>
          </ul>
        </article>
        <article class="command-card">
          <p class="section-kicker">建议演示命令</p>
          <h3>答辩时可以这样引导现场</h3>
          <ul class="command-list">
            <li>先运行 `bash dashboard/scripts/start_monitor.sh`，直接进入 xv6 交互。</li>
            <li>在 xv6 里执行 `ps`、`scheduler_test`、`mmap_test` 观察四条主线。</li>
            <li>需要兜底排障时再切到 `bash dashboard/scripts/start_monitor.sh --split-console`。</li>
          </ul>
        </article>
      </section>
    </section>
  `;

  bindRouteButtons(root, actions);
}
