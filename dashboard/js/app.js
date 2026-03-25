const formatPercent = (value) => value == null ? 'N/A' : `${value.toFixed(1)}%`;
const formatNumber = (value) => value == null ? 'N/A' : `${value}`;

async function loadJson(path, fallback) {
  try {
    const res = await fetch(path, { cache: 'no-store' });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return await res.json();
  } catch (_) {
    return fallback;
  }
}

function createCard(title, value, extra) {
  const card = document.createElement('article');
  card.className = 'summary-card';
  card.innerHTML = `<p>${title}</p><strong>${value}</strong>${extra ? `<em>${extra}</em>` : ''}`;
  return card;
}

function renderSummary(results) {
  const container = document.getElementById('summary-cards');
  container.innerHTML = '';
  const summary = results.summary || {};
  const improvements = summary.improvements || {};

  container.appendChild(createCard('Testing 完整度', `${summary.testing_valid || 0}/${summary.testing_expected || 0}`, '模块三关键指标有效项'));
  container.appendChild(createCard('COW 最佳场景', formatPercent(improvements.cow_best_case), 'fork 后不访问内存'));
  container.appendChild(createCard('Lazy 稀疏访问', formatPercent(improvements.lazy_sparse), '访问 1% 页面'));
  container.appendChild(createCard('COW 只读场景', formatPercent(improvements.cow_readonly), 'fork 后共享只读页面'));
  container.appendChild(createCard('调度分离度', formatPercent(summary.scheduler_spread_score), '越高说明越能拉开场景差异'));
  container.appendChild(createCard('基线有效项', `${summary.baseline_valid || 0}/${summary.baseline_expected || 0}`, '用于对比的 baseline 数据'));
}

function sparkline(points, color) {
  if (!points.length) {
    return '<p class="note">暂无快照数据</p>';
  }
  const width = 320;
  const height = 150;
  const padding = 12;
  const max = Math.max(...points);
  const min = Math.min(...points);
  const span = Math.max(1, max - min);
  const coords = points.map((value, index) => {
    const x = padding + (index * (width - padding * 2)) / Math.max(1, points.length - 1);
    const y = height - padding - ((value - min) * (height - padding * 2)) / span;
    return `${x},${y}`;
  }).join(' ');
  return `
    <svg viewBox="0 0 ${width} ${height}" preserveAspectRatio="none">
      <rect x="0" y="0" width="${width}" height="${height}" rx="18" fill="rgba(24,34,47,0.03)"></rect>
      <polyline points="${coords}" fill="none" stroke="${color}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"></polyline>
    </svg>
    <div class="axis-labels"><span>${min}</span><span>${max}</span></div>
  `;
}

function renderLive(snapshotsPayload) {
  const samples = snapshotsPayload.samples || [];
  document.getElementById('live-status').textContent = samples.length ? `${samples.length} samples` : 'No data';

  document.getElementById('cpu-chart').innerHTML = sparkline(samples.map(item => item.cpu), '#b84c2a');
  document.getElementById('mem-chart').innerHTML = sparkline(samples.map(item => item.free_pages), '#0d6e6e');
  document.getElementById('rq-chart').innerHTML = sparkline(samples.map(item => item.runqueue), '#1f4fbf');

  const latest = samples[samples.length - 1];
  const liveStats = document.getElementById('live-stats');
  liveStats.innerHTML = '';
  if (!latest) {
    liveStats.innerHTML = '<p class="note">运行 `sysmon` 并导入快照后，这里会显示最新系统状态。</p>';
  } else {
    [
      ['Scheduler', latest.sched],
      ['CPU Usage', `${latest.cpu}%`],
      ['Free Pages', latest.free_pages],
      ['Runqueue', latest.runqueue],
      ['COW Faults', latest.cow_faults],
      ['Lazy Allocs', latest.lazy_allocs],
    ].forEach(([label, value]) => {
      const cell = document.createElement('div');
      cell.innerHTML = `<span>${label}</span><strong>${value}</strong>`;
      liveStats.appendChild(cell);
    });
  }

  const tbody = document.getElementById('process-table');
  tbody.innerHTML = '';
  const processes = snapshotsPayload.latest_processes || [];
  if (!processes.length) {
    const row = document.createElement('tr');
    row.innerHTML = '<td colspan="7" class="note">暂无进程快照</td>';
    tbody.appendChild(row);
    return;
  }

  processes
    .slice()
    .sort((a, b) => (b.rutime || 0) - (a.rutime || 0))
    .forEach((proc) => {
      const row = document.createElement('tr');
      row.innerHTML = `
        <td>${proc.pid}</td>
        <td>${proc.name}</td>
        <td>${proc.state}</td>
        <td>${proc.sched}</td>
        <td>${proc.rutime}</td>
        <td>${proc.retime}</td>
        <td>${proc.stime}</td>
      `;
      tbody.appendChild(row);
    });
}

async function main() {
  const emptySnapshots = { samples: [], latest_processes: [], summary: {} };
  const emptyResults = { summary: {}, baseline: {}, testing: {} };

  const [results, snapshots] = await Promise.all([
    loadJson('data/results.json', emptyResults),
    loadJson('data/snapshots.json', emptySnapshots),
  ]);

  document.getElementById('results-status').textContent = results.generated_at ? 'Ready' : 'Missing';
  renderSummary(results);
  renderLive(snapshots);
}

main();
