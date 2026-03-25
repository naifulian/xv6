# 模块三目录说明

`experiments/` 是模块三的正式目录，负责实验采集、校验、聚合、出图和论文产物输出。

目录约定：

- `collect/`：采集入口脚本
- `analyze/`：聚合、校验和绘图脚本
- `logs/`：原始实验日志与多轮 run 数据
- `outputs/data/`：JSON / CSV 汇总结果
- `outputs/figures/`：PNG / PDF 图表
- `outputs/reports/`：Markdown / LaTeX 报告

推荐入口：

```bash
bash experiments/collect/collect_data.sh all --rounds 3 --resume
python3 experiments/analyze/validate_logs.py
python3 experiments/analyze/plot_results.py
```

兼容说明：

- `webui/collect_data.sh`
- `webui/analyze_all.sh`
- `webui/validate_logs.py`
- `webui/plot_results.py`

这些旧路径目前仍可用，但仅作为过渡兼容入口，不再是模块三正式目录。
