# Module Three Experiment Report

- Generated at: 2026-03-25T17:36:18.734656
- Baseline runs detected: 3
- Testing runs detected: 3
- Error bars use cross-round 95% CI only when at least two valid rounds are available; otherwise plots show descriptive in-run spread and the comparison table is labeled `insufficient-rounds`.
- Mixed unit/batch/runs signatures are treated as invalid datasets and must be recollected or split before inferential comparison.
- Raw in-run sample series are retained in JSON/CSV for appendix tables or follow-up significance checks, while the primary charts still use cross-round summaries over boot-level repetitions.
- Data quality score: 76.93/100

## Data Quality

- Coverage: 100.0%
- Repeatability: 100.0%
- Scheduler discrimination score: 64.7
- Stability score: 20.0
- Mixed-signature items: 0
- Low-resolution complete runs: 0
- High-variation items: 16

## Experiment Configuration

- Baseline runs discovered: 3
- Baseline complete runs: 3
- Baseline partial runs: 0
- Baseline raw-only recovered runs: 0
- Baseline commit(s): b974bf7bc8888d251e682355e443d279879a1de4-dirty
- Baseline command(s): perftest memory
- Baseline CPU setting(s): 1

- Testing runs discovered: 3
- Testing complete runs: 3
- Testing partial runs: 0
- Testing raw-only recovered runs: 0
- Testing commit(s): 9b20e87fe39d9631cdedbd93f1b30df6655e1c73-dirty
- Testing command(s): perftest memory + perftest sched_core + perftest sched_fairness + perftest sched_response
- Testing collection mode: split memory/scheduler boots
- Testing CPU setting(s): 1

## Memory Optimization Summary

- Formal thesis memory conclusions in this version are limited to COW and Lazy Allocation.
- Buddy and mmap results may still appear in raw JSON/logs for compatibility, but they are not part of the formal comparison table or thesis claims.

| Test | Baseline | Testing | Delta | Interval | Interpretation |
| --- | ---: | ---: | ---: | --- | --- |
| COW No Access | 156 | 9.67 | 93.8% | separated | significant improvement |
| COW Read Only | 167 | 10.33 | 93.8% | separated | significant improvement |
| COW Write 30% | 10 | 9 | 10.0% | overlap | difference not clearly separated |
| COW Write 100% | 10 | 28.67 | -186.7% | separated | clear counter-example |
| Lazy Access 1% | 225.33 | 10.33 | 95.4% | separated | significant improvement |
| Lazy Access 50% | 4.33 | 6.33 | -46.2% | separated | clear counter-example |
| Lazy Access 100% | 4 | 13 | -225.0% | separated | clear counter-example |

## Scheduler Scenario Summary

| Scenario | Winner | Spread | Note |
| --- | --- | ---: | --- |
| throughput | RR | 53.8% | lower completion time is better |
| convoy | RR | 52.8% | lower short-job completion time is better |
| fairness | RR | 94.9% | lower CPU-time spread is fairer |
| response | MLFQ | 57.2% | lower interactive latency is better |

## Counterexamples Worth Discussing

- COW Write 100%: -186.7% relative to baseline.
- Lazy Access 50%: -46.2% relative to baseline.
- Lazy Access 100%: -225.0% relative to baseline.

## High-Variation Items

- baseline:COW_FULLWRITE -> CV=17.32%
- baseline:COW_NO_ACCESS -> CV=10.01%
- baseline:COW_PARTIAL -> CV=10.00%
- baseline:COW_READONLY -> CV=18.71%
- baseline:LAZY_HALF -> CV=13.32%
- testing:COW_FULLWRITE -> CV=10.66%
- testing:COW_NO_ACCESS -> CV=11.95%
- testing:LAZY_SPARSE -> CV=14.78%
- testing:SCHED_CONVOY_FCFS -> CV=28.87%
- testing:SCHED_CONVOY_RR -> CV=35.66%
- testing:SCHED_CONVOY_SRTF -> CV=10.83%
- testing:SCHED_FAIRNESS_CFS -> CV=13.43%
- testing:SCHED_RESPONSE_RR -> CV=24.74%
- testing:SCHED_THROUGHPUT_FCFS -> CV=10.73%
- testing:SCHED_THROUGHPUT_RR -> CV=13.73%
- testing:SCHED_THROUGHPUT_SJF -> CV=20.99%

## Output Artifacts

- `experiments/outputs/data/results.json`
- `experiments/outputs/data/results_long.csv`
- `experiments/outputs/data/round_results.csv`
- `experiments/outputs/data/comparison_summary.csv`
- `experiments/outputs/data/scheduler_summary.csv`
- `experiments/outputs/reports/experiment_report.md`
- `experiments/outputs/reports/thesis_memory_table.tex`
- `experiments/outputs/figures/*.png` / `experiments/outputs/figures/*.pdf`
