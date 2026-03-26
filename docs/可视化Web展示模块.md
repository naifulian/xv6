xv6 运行态可视化 Web 展示模块
重构开发文档
围绕模块一的统一调度框架与统一地址空间语义
面向任务管理器式运行态展示

文档摘要
本文将模块四重新定义为“xv6 运行态可视化与答辩展示系统”，核心目标是做一个独立于模块三实验管线的任务管理器式 Web 界面。
模块四直接服务模块一：重点展示统一调度框架在运行时的决策行为，以及统一地址空间语义在 COW、Lazy Allocation、匿名私有 mmap 间的协同行为。
虽然系统中实现了 Buddy 物理页分配器，但它不再作为论文主线和模块四主展示对象；如保留相关计数，也仅作为调试信息，不进入答辩主叙事。
同时，现有把模块三脚本、实验产物和前端页面全部塞进 `webui/` 的组织方式不再推荐。后续应将模块三与模块四彻底拆分，形成“实验管线”和“运行态展示”两条独立工程线。

1. 重定义后的模块边界
模块一负责系统机制本体：统一调度框架、统一地址空间语义、观测接口和必要统计字段。
模块二负责测试与回归，证明这些机制已经正确实现并能稳定运行。
模块三负责实验设计、批量采集、数据校验、聚合出图和论文结果产物。
模块四负责运行态可视化，不消费模块三作为核心依赖，而是直接消费 xv6 运行时观测数据，形成独立的展示与答辩平面。

这意味着模块四不再是“实验结果网页壳子”，而是一个独立的运行时 dashboard。模块三图表和论文结果如果需要保留，应视为附加入口，而不应继续主导模块四的数据模型、目录结构和页面组织。

2. 当前问题与重构结论
当前实现最主要的问题有三个：
• `webui/` 同时承担模块三脚本目录、实验产物目录、静态网页目录和监控数据目录，职责混杂。
• 现有模块四页面高度依赖模块三的 `results.json`、图表文件和实验产物，导致“运行态展示”反而退化成“实验结果展示”。
• 页面形态更像论文结果页，而不是你真正想要的“任务管理器式系统观察界面”。

因此，重构结论如下：
• 模块四应独立于模块三落地，核心链路为“xv6 运行态遥测 -> 宿主机 bridge -> 浏览器 dashboard”。
• 模块三应从 `webui/` 迁出，形成单独的实验目录与产物目录。
• 模块四 UI 应围绕系统总览、进程主表、调度器视图、内存语义视图、事件流这五类能力展开。
• Buddy 不进入模块四主界面，不作为论文展示重点。

3. 目录重构方案
现有 `webui/` 目录的问题，不在于名字本身，而在于它把“实验”和“展示”混成了一个模块。后续建议按下面方式拆分。

推荐目标结构：

```text
xv6/
├── experiments/                 # 模块三：实验采集与分析
│   ├── collect/
│   │   └── collect_data.sh
│   ├── analyze/
│   │   ├── experiment_data.py
│   │   ├── validate_logs.py
│   │   └── plot_results.py
│   ├── outputs/
│   │   ├── data/
│   │   ├── figures/
│   │   └── reports/
│   └── README.md
├── dashboard/                   # 模块四：运行态展示系统
│   ├── bridge/
│   │   ├── app.py
│   │   ├── parser.py
│   │   └── state.py
│   ├── frontend/
│   │   ├── index.html
│   │   ├── css/
│   │   └── js/
│   ├── scripts/
│   │   └── run_dashboard.sh
│   └── runtime/
│       ├── dashboard-telemetry.log
│       └── xv6-console.log
├── user/
│   └── dashboardd.c
└── docs/
    └── module4/
```

从当前仓库迁移时，建议做如下映射：
• `webui/collect_data.sh` -> `experiments/collect/collect_data.sh`
• `webui/experiment_data.py` -> `experiments/analyze/experiment_data.py`
• `webui/validate_logs.py` -> `experiments/analyze/validate_logs.py`
• `webui/plot_results.py` -> `experiments/analyze/plot_results.py`
• `webui/data/`、`webui/figures/`、`webui/reports/` -> `experiments/outputs/`
• `webui/index.html`、`webui/css/`、`webui/js/` -> `dashboard/frontend/`
• `webui/parse_sysmon.py` -> `dashboard/bridge/parser.py`

短期兼容策略：
• 在目录迁移完成前，可以保留 `webui/` 作为过渡目录。
• 但从现在开始，不再往 `webui/` 新增模块三和模块四混合文件。
• 新开发的模块四代码应直接放在 `dashboard/` 下。

4. 模块四的定位
模块四的目标不是替代模块三做实验，也不是简单展示论文图表，而是把 xv6 的运行态变成一个可实时观察、可答辩讲解、可辅助调试的系统界面。

从论文叙事上，模块四直接服务模块一：
• 调度主线：统一调度框架如何在 RR、FCFS、PRIORITY、SML、LOTTERY、SJF、SRTF、MLFQ、CFS 之间体现运行时差异。
• 地址空间主线：COW、Lazy Allocation、匿名私有 mmap 如何在统一地址空间语义下协同工作。

模块四不再依附模块三，也不应把模块三产物当成主数据源。模块三属于离线实验世界，模块四属于在线运行态世界，两者可以并存，但应明确分层。

5. 设计原则
• Web 服务放在宿主机，不放在 xv6 内部。xv6 只提供可信、轻量、结构化的运行态数据。
• 界面风格优先接近“教学型任务管理器”，而不是论文图表页或炫技大屏。
• 优先展示运行时真实状态，不伪造 Linux 级性能指标。
• 优先保证快照一致性与系统稳定性，再追求刷新频率。
• 所有字段口径必须诚实，例如 `sz` 只能称为“逻辑地址空间规模”，不能伪装为 RSS。
• Buddy 不进入主界面，不作为 KPI，不作为答辩主叙事。

6. 总体架构
推荐继续采用三层架构：

第 1 层（Guest / xv6）
轻量观测代理 `dashboardd`，负责周期性采样运行态信息并输出结构化遥测帧。

第 2 层（Host / Bridge）
宿主机 bridge 服务负责读取日志或字符设备、解析遥测帧、维护最新状态、提供 WebSocket 与 REST 接口。

第 3 层（Browser / UI）
浏览器 dashboard 负责展示系统总览、进程表、调度视图、内存视图和事件流。

推荐数据流：

```text
xv6 kernel counters / snapshot interfaces
-> dashboardd
-> telemetry frames
-> host bridge
-> websocket / rest
-> browser dashboard
```

7. xv6 侧实现策略
为了减少重构风险，模块四不建议一开始就发明复杂协议或一次性补齐所有内核接口，而应分阶段推进。

7.1 Phase 1：复用现有观测接口
先基于现有 `getsnapshot()`、`getptable()`、`getmemstat()` 做最小可运行版。
这一阶段目标是先把下面四类信息稳定展示出来：
• 系统级摘要：ticks、policy、context switches、RUNNABLE 数、free pages
• 进程主表：pid、name、state、priority、tickets、rutime、retime、stime、sz
• 调度器基础字段：当前策略、运行队列长度、每个进程的基础调度属性
• 内存基础字段：free pages、COW fault、Lazy fault、逻辑地址空间规模

7.2 Phase 2：补最少量新增字段
当 MVP 稳定后，再补真正影响“任务管理器感”的字段：
• `running_pid`
• `heap_end`
• `vma_count`
• `mmap_regions`
• `queue_level`（MLFQ）
• `remaining_time`（SRTF）
• `vruntime`（CFS）

这里建议优先做“扩展已有结构”或“新增聚合接口”，而不是一开始就做大量分散的新系统调用。

7.3 Phase 3：进程详情接口
最后再补按需读取的详情接口，例如：
• `getdashframe(dst)`：聚合一次完整展示帧
• `getprocvmas(pid, dst, max)`：读取选中进程 VMA 明细

这两个接口值得做，但不应阻塞模块四 MVP 上线。

8. telemetry 协议建议
模块四要独立于模块三，但不需要在 guest 侧一开始就承担太重的协议复杂度。

推荐策略：
• Phase 1 继续采用轻量文本协议，延续现有 `SNAP/PROC` 的思路。
• Phase 2 在链路稳定后，再升级为带前后缀的 framed payload。
• 如确实需要 JSON，建议放到宿主机 bridge 侧生成，而不是让 xv6 用户态守护进程承担复杂 JSON 拼装。

推荐协议演进路线：
• V1：`SNAP` / `PROC` 行式结构化文本，便于调试和快速落地。
• V2：`@@XV6DASH_BEGIN@@ ... @@XV6DASH_END@@` 分帧文本。
• V3：摘要帧 + 完整帧 + 事件帧混合输出。

这样做的原因很简单：xv6 用户态资源紧张，小栈环境下应优先选择稳妥协议，而不是过早把复杂度压进 guest 侧。

9. Host Bridge 设计
Host Bridge 是模块四的核心宿主机组件，职责如下：
• 默认读取分离后的 telemetry 日志，也支持按需接入原始控制台日志或其他独立遥测通道
• 解析 `dashboardd` 输出的结构化帧
• 维护 latest summary / latest processes / selected process / recent events
• 对前端提供 WebSocket 实时推送
• 对前端提供 REST 查询接口

推荐 API：
• `/ws/live`：推送 summary / process / event
• `/api/summary`：读取系统总览
• `/api/processes`：读取最新进程快照
• `/api/processes/{pid}`：读取选中进程详情
• `/api/events`：读取最近事件
• `/api/policy`：返回当前策略说明与字段解释

控制接口例如切换调度器、kill 进程、修改优先级，应视为 V2 或 V3 能力，不进入第一阶段验收。

10. QEMU 与启动链设计
推荐默认使用“交互控制台 + telemetry 分流文件”的启动链：

```text
QEMU stdio/serial
-> split_console.py
-> xv6-console.log                 # 原始控制台回放与调试
-> dashboard-telemetry.log         # bridge 默认消费的结构化 telemetry
-> host bridge tail/parse
```

默认策略：
• `dashboard/scripts/capture_xv6_console.sh` 保持 shell 可交互，并把 telemetry 从控制台输出里分离出来。
• `dashboard/scripts/run_dashboard.sh` 与 bridge 默认读取 `dashboard/runtime/dashboard-telemetry.log`。
• `xv6-console.log` 只保留给调试、问题复盘和原始终端回放，不作为 dashboard 默认数据源。

这样做的原因是：
• 避免 shell 回显、用户输入和普通控制台输出污染 telemetry 解析结果
• 保留交互式 xv6 终端，不牺牲开发体验
• 让 bridge 默认消费单一、结构化、可持续 tail 的日志文件

增强版路线：
• 后续可以继续把 telemetry 分离到独立串口、pty、socket 或 virtconsole
• 但这些链路属于增强项；在当前版本里，默认分流文件已经是基线方案，而不是临时调试手段

关于自启动：
• `dashboardd` 最终可以由 `init` 自动拉起
• 但应放在展示链路稳定之后再接入
• 第一阶段允许手工启动，优先把 UI、bridge 和数据链打通

11. 任务管理器式 UI 设计
模块四界面应以“单页任务管理器 + 右侧详情”作为主结构，而不是做成论文图表展板。

推荐页面结构：
• 顶部状态栏：online/offline、当前策略、ticks、seq、refresh interval
• KPI 区：总进程数、RUNNABLE、当前运行 PID、free pages、COW faults、Lazy faults、mmap regions
• 左侧主区：进程主表
• 右侧详情：选中进程的运行统计、地址空间摘要、VMA 列表
• 底部区域：调度器专属视图 + 事件流

11.1 进程主表
这是模块四最重要的页面核心，字段建议如下：
• pid
• name
• state
• priority
• tickets
• rutime
• retime
• stime
• sz
• queue_level（若有）
• remaining_time（若有）
• vruntime（若有）

11.2 调度器专属视图
不同策略不需要共用一套图，而应按算法本质展示差异：
• FCFS：到达顺序与执行顺序
• PRIORITY：优先级对比
• LOTTERY：tickets 分布
• SRTF：remaining_time
• MLFQ：queue_level
• CFS：vruntime

11.3 内存视图
内存视图要聚焦统一地址空间语义，不要退回成“论文实验图表区”。
推荐展示：
• free pages
• COW fault 计数
• Lazy fault 计数
• mmap region 数
• 选中进程的 `sz`、`heap_end`
• 选中进程的 VMA 列表

不推荐展示：
• Buddy 专属面板
• 完整页表明细
• 伪 RSS
• 伪 CPU profiling 图

11.4 事件流
事件流是答辩最有讲解价值的区域，建议优先覆盖：
• fork
• exit
• scheduler switch
• lazy fault
• cow fault
• mmap
• munmap

若内核级逐事件采集成本过高，可先采用“快照差分 + 计数器差分”生成近实时事件。

12. 模块四与模块三的关系
模块四重做后，原则上不再依赖模块三。

两者关系应重写为：
• 模块三：离线实验、正式结果、论文图表
• 模块四：在线运行态观察、答辩演示、开发调试

如果未来仍想保留“论文结果页面”，建议作为独立附加入口处理：
• 方案 A：放入 `experiments/viewer/`
• 方案 B：在 `dashboard/` 中保留只读附录页，但不计入模块四核心范围

无论采用哪种方案，都不应让 `results.json`、实验图表和论文产物继续主导模块四的主页面组织。

13. 答辩展示主线
模块四答辩时最值得展示的是两条主线：

调度主线
• 切换 RR、FCFS、PRIORITY、MLFQ、CFS
• 观察当前策略、运行队列变化、进程调度差异
• 在专属视图中解释算法字段

地址空间主线
• 运行 COW、Lazy、mmap 相关专项程序
• 观察 free pages、fault 计数、VMA 列表变化
• 强调统一地址空间语义，而不是泛泛展示“内存变了”

不建议在答辩主线上强调 Buddy。Buddy 可以作为系统实现细节存在，但不应成为模块四展示重点，更不应重新抬升为论文主线。

14. 分阶段落地计划
Phase 0：目录拆分
目标：把模块三和模块四从 `webui/` 中拆开
产出：`experiments/`、`dashboard/` 目录骨架与迁移清单

Phase 1：独立 runtime MVP
目标：只依赖 xv6 运行时观测接口，完成总览 + 进程主表
产出：`dashboardd`、bridge、summary 页面、process table 页面

Phase 2：任务管理器增强
目标：完成右侧进程详情、调度器专属视图
产出：MLFQ/CFS/SRTF 字段展示、策略说明面板

Phase 3：内存语义增强
目标：完成 VMA 与地址空间视图
产出：`heap_end`、VMA 列表、mmap/COW/Lazy 相关状态面板

Phase 4：答辩强化
目标：完成自启动、演示脚本、字段说明、口径说明
产出：`run_dashboard.sh`、演示文档、稳定版界面

15. 验收标准
• 启动模块四后，浏览器在 xv6 启动完成后 3 秒内能看到在线状态。
• 模块四核心页面不依赖模块三 `results.json`、论文图表和实验产物。
• 至少包含系统总览、进程主表、调度视图、内存视图、事件流五个核心区域。
• 界面整体风格接近任务管理器，而不是论文结果展板。
• Buddy 不作为主 KPI，不占据主界面核心区域。
• 模块三目录迁出 `webui/` 后，模块四仍能单独运行。

16. 论文写法建议
论文中可将模块四表述为：
“在模块一统一调度框架与统一地址空间语义基础上，设计并实现了一个独立的运行态可视化展示系统。该系统通过 guest 侧轻量观测代理、宿主机 bridge 服务与浏览器前端三层结构，将调度与地址空间机制的关键状态以任务管理器式界面实时呈现，用于答辩展示、运行态调试与教学解释。”

建议明确写清三点：
• 模块四服务模块一，而不是模块三的附属页面。
• 模块三负责离线实验方法与正式结果，模块四负责在线运行态可视化。
• Buddy 虽然完成实现，但不作为论文主展示和模块四主叙事。

17. 最终执行建议
推荐按下面顺序推进：
先拆目录，再重做模块四；
先做独立 runtime MVP，再做调度和内存增强；
最后再处理自启动、演示脚本和答辩包装。

如果你要的就是“更像任务管理器、而不是实验结果页”的模块四，那么正确路线就是：
让模块四从模块三中独立出来，让 `webui/` 退场，把展示核心重新放回 xv6 运行态本身。
