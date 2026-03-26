# BUG-021: 模块四启动卡住是 telemetry 链路阻塞

## 基本信息

| 项目 | 内容 |
|------|------|
| 编号 | BUG-021 |
| 分类 | 用户工具 (USER) |
| 严重程度 | 高 |
| 初次记录时间 | 2026-03-26 |
| 影响文件 | `kernel/sysproc.c`, `kernel/telemetry_console.c`, `user/dashboardd.c`, `dashboard/css/style.css` |

## 现象与触发条件

模块四的推荐启动命令是：

```bash
bash dashboard/scripts/start_monitor.sh
```

问题出现时，终端表象是：

```text
xv6 kernel is booting

init: started dashboardd
init: starting sh
```

与此同时，Web 页面长期停留在“初始化中”，或者只能看到非常少的早期状态，和预期的实时监控界面不一致。

## 影响范围

1. 模块四首页无法稳定展示 live monitor 数据。
2. 容易误判成 xv6 启动卡住，或者误判成 `init` / `sh` 本身异常。
3. 如果只看 shell 表象，会和 [BUG-014](/home/niya/xv6/docs/bug/BUG-014-proc结构体添加字段导致启动卡住.md) 的启动卡住现象混淆。

## 复现与观察

排查时最关键的两个观察是：

1. `init: starting sh` 之后并不代表系统挂了，`sh` 只是已经进入前台等待输入。
2. 真正的问题在 telemetry 链路，`dashboardd` 进入后很快就不再产生正常的 `SNAP/PROC` 记录。

进一步检查宿主机日志后发现：

```text
DASHBOARDD_BEGIN interval=10 samples=0 mode=stream
```

后面没有稳定持续的样本输出，说明卡点发生在 `dashboardd` 的采样/发射路径中，而不是单纯的 shell 提示符问题。

## 排查过程

1. 先对照 [BUG-014](/home/niya/xv6/docs/bug/BUG-014-proc结构体添加字段导致启动卡住.md)，怀疑仍然是“启动期进程调度链”出了问题。
2. 重新核对 `init.c`，确认 `dashboardd` 确实已被拉起，`sh` 也已开始运行，所以系统并没有停在 `init` 的 fork/exec 阶段。
3. 检查 `dashboardd` 的输出链，发现它依赖 `telemetry_printf()`，而 `telemetry_printf()` 最终会走 `telemetry_console_write()`。
4. 查看 `kernel/telemetry_console.c` 后确认，写路径里存在等待 `used->idx` 回收的自旋逻辑，首包一旦回收时序不对就可能把 `dashboardd` 卡在第一条 telemetry 上。
5. 继续看 `sys_getptable()`，发现它原先把最多 64 条 `struct pstat` 塞进一页临时缓冲区，这在当前字段规模下存在越界风险，也会放大“启动后监控不动”的表象。
6. 为了确认问题链路，临时给 `dashboardd` 加了阶段日志，并用短时启动验证，最终把卡点缩小到 telemetry 首包和 `getptable()` 的实现方式。

## 根因结论

这次真正的根因不是 [BUG-014](/home/niya/xv6/docs/bug/BUG-014-proc结构体添加字段导致启动卡住.md) 那类调度 ABI 误判，而是两处更靠后的监控链路问题叠加：

- `sys_getptable()` 的实现方式还停留在“单页临时缓冲区”思路上，面对 64 条进程记录时不够稳妥。
- `telemetry_console_write()` 对 virtconsole 回收状态的等待过于激进，容易让 `dashboardd` 在首条遥测上阻塞。

因此，表面看起来像“启动卡住”，实际是“模块四的观测链路先被自己卡住了”。

## 修复过程

1. 将 `sys_getptable()` 改成逐条 `copyout()` 到用户缓冲区，不再依赖单页临时页承载整张表。
2. 将 `telemetry_console_write()` 的回收等待改成非阻塞更新，避免第一条遥测把 `dashboardd` 锁死。
3. 同时把首页的导航和入口按钮改成响应式网格，保证模块四在不同窗口宽度下都能正常把按钮排开，不挤压主内容。
4. 在排障阶段短暂加入了 `dashboardd` 的阶段日志，确认问题确实不在 shell 启动链，而在 telemetry 首包和样本采集路径。验证完成后已移除这些临时日志。

## 提交前后验证

修复前的验证现象是：

- `xv6 kernel is booting`
- `init: started dashboardd`
- `init: starting sh`
- Web 端仍停在初始化态，或者样本迟迟不增长

修复后的验证结果是：

- `dashboardd` 能持续输出 `SNAP/PROC` 记录
- `/api/dashboard` 返回 `online: true`，并开始出现样本
- 模块四页面不再停留在“初始化中”

我在 2026-03-26 的验证里确认过一次完整链路：监控脚本启动后，`dashboardd` 可以连续采样，桥接接口也能正常解析出 live 数据。随后又重新跑了一次无调试输出的镜像，确认日常版本也能正常工作。

## 和 BUG-014 的区别

这次很像 [BUG-014](/home/niya/xv6/docs/bug/BUG-014-proc结构体添加字段导致启动卡住.md)，但根因不是同一类：

- BUG-014 重点是“启动期误判成 `proc` 扩容 / ABI 问题”。
- BUG-021 重点是“模块四 telemetry 链路自己阻塞了，导致观测层看起来像启动卡住”。

这也是为什么这次不能只看 `sh` 的表象，必须同时检查宿主机 telemetry 文件和 `dashboardd` 的采样输出。

## 经验结论

1. `init: starting sh` 只代表 shell 已起来，不等于监控链路已经健康。
2. 模块四的“卡住”首先要怀疑观测链路，而不是先怀疑 xv6 主启动链。
3. 处理结构化监控数据时，必须把缓冲区尺寸、copyout 策略和设备回收时序一起考虑。
4. 复盘文档里要保留“提交前后验证过程”，这样后面再遇到类似现象时，能迅速区分是旧 bug 复发还是新链路问题。
