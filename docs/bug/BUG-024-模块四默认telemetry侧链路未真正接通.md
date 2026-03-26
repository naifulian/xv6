
# BUG-024: 模块四默认 telemetry 侧链路未真正接通

## 基本信息

| 项目 | 内容 |
|------|------|
| 编号 | BUG-024 |
| 分类 | 用户工具 (USER) |
| 严重程度 | 高 |
| 初次记录时间 | 2026-03-26 |
| 影响文件 | `Makefile`, `kernel/telemetry_console.c`, `user/telemetry_printf.c`, `user/dashboardd.c`, `user/sysmon.c`, `dashboard/scripts/run_dashboard.sh` |

## 问题描述

虽然模块四早就有 telemetry side channel 的设计思路，但在默认产品路径里，它并没有真正接通成“开箱即用”的链路。

修复前的状态是：

- QEMU 默认启动参数里没有稳定挂好 telemetry 所需设备。
- `dashboardd` 和 `sysmon` 还在走 `printf(...)` / stdout 路径。
- 结果就是默认链路仍然需要手工补参数，或者继续依赖 `split_console.py` 这种兼容分流方案。

## 复现现象

修复前，如果不手工补配置，模块四经常表现为：

- telemetry 文件没有稳定产出。
- 前端依赖的快照数据不完整。
- 默认启动体验仍然像“实验样机”，不是正式入口。

## 排查过程

1. 检查 `Makefile`，发现 telemetry console 相关设备挂载并没有成为默认链路的一部分。
2. 检查 `user/dashboardd.c` 和 `user/sysmon.c`，发现结构化输出并没有统一切到 `telemetry_printf(...)`。
3. 对照 `dashboard/scripts/run_dashboard.sh`，确认 bridge 和 guest 的默认产品路径还是不够闭合。
4. 在提交 `6dd1b90` 后，这条链路才真正补成“默认可用”的状态。

## 根因结论

根因不是 telemetry 概念不存在，而是它只停留在“有实现、有接口、但没成为默认路径”的阶段。

换句话说，模块四在这之前是“能手工拼出来”，但还不是“默认开箱即用”。

## 修复方案

1. 在 `Makefile` 里把 telemetry side channel 设备和相关依赖纳入默认启动配置。
2. 将 `dashboardd` 与 `sysmon` 的结构化输出统一切换为 `telemetry_printf(...)`。
3. 在文档中明确 default path 与 fallback/debug path 的边界。

## 修复后验证

修复后已确认：

- 默认 QEMU 启动链路已经能挂载 telemetry side channel。
- `telemetrywrite()` 不再需要手工补额外参数才能走通。
- 模块四的默认路径终于从“兼容层”变成“正式入口”。

## 经验结论

1. 有功能不等于默认可用。
2. 对展示层来说，默认链路是否闭合比单点实现更重要。
3. 如果默认路径还要靠手工补参，那就说明 bug 还没真正修完。
