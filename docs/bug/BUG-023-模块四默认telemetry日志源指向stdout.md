
# BUG-023: 模块四默认 telemetry 日志源指向 stdout

## 基本信息

| 项目 | 内容 |
|------|------|
| 编号 | BUG-023 |
| 分类 | 用户工具 (USER) |
| 严重程度 | 高 |
| 初次记录时间 | 2026-03-26 |
| 影响文件 | `dashboard/bridge/app.py`, `dashboard/scripts/run_dashboard.sh`, `dashboard/runtime/dashboard-telemetry.log` |

## 问题描述

在这次修复之前，bridge 的默认 source 仍然偏向 `xv6-console.log` 或者其它普通控制台回放源，而不是模块四真正应该消费的 `dashboard/runtime/dashboard-telemetry.log`。

这样做的直接问题是：

- shell 回显、用户输入和普通控制台输出会混进解析结果。
- 监控数据看起来像“还能跑”，但默认数据源其实不对。
- 一旦控制台输出格式变化，前端就会跟着一起误判。

## 复现现象

修复前，bridge 往往会把控制台文本当作默认数据源，结果表现为：

- 页面数据与真实 telemetry 不一致。
- 解析结果里夹杂控制台噪声。
- 调试时很容易把“控制台输出”误当成“遥测帧”。

## 排查过程

1. 先看 `dashboard/bridge/app.py`，发现默认 source 仍然不是 telemetry 专用日志。
2. 再对照 `dashboard/scripts/run_dashboard.sh`，发现默认启动链路已经准备好了 telemetry side channel，但 bridge 没有真正吃到这个默认产物。
3. 结合模块四审查记录，确认问题的本质不是 bridge 无法解析，而是“默认数据源选错了”。
4. 最终在提交 `f142439` 中把默认 source 统一到 `dashboard/runtime/dashboard-telemetry.log`。

## 根因结论

根因是模块四早期把“控制台回放”和“遥测消费”混在了一起，导致默认 source 仍然落在 stdout / console 回放路径上。

## 修复方案

1. 将 bridge 默认 source 改为 `dashboard/runtime/dashboard-telemetry.log`。
2. 明确 `xv6-console.log` 只用于调试和原始回放，不再作为 dashboard 默认输入。
3. 在文档中同步说明默认链路，避免后续维护时再把控制台回放误认为正式数据源。

## 修复后验证

修复后已确认：

- bridge 默认读取 telemetry 专用日志。
- shell 回显不再污染默认监控数据。
- 文档与代码对默认 source 的描述一致。

## 经验结论

1. 监控系统的默认 source 一旦选错，后面所有可视化都可能“看起来对、实际上错”。
2. 先分清控制台回放和遥测消费，是模块四里最基础的边界。
3. 默认入口必须和默认数据源同时收口，不能只修一半。
