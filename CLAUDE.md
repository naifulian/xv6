# CLAUDE.md

本文件为 Claude Code (claude.ai/code) 在此代码库工作时提供指导，相关文档在 docs 目录中

<<回答必须使用中文!!!>>

<<不要使用 sed -i 的方式和正则表达式等 shell 工具偷懒修改代码>>

<\<CLAUDE.md 和 docs的文档不要 commit>>

<<修改过的 bug commit 之后记录在 docs/log 目录下>>
<<写完测试代码后告诉我如何测试，我来手动测试>>

<<不要随便写脚本，除非我让你写>>

***

## 项目概述

本项目是对 MIT xv6-riscv 教学操作系统的深度扩展，作为本科生的毕设，这个过程需要考虑qemu模拟器和xv6的限制。分为四个主要模块：
### 模块一：调度算法优化 + 内存管理优化

**调度算法：** 新实现了 8 种进程调度算法，支持运行时动态切换

- DEFAULT (Round-Robin)
- FCFS (先来先服务)
- PRIORITY (优先级调度)
- SML (静态多级队列)
- LOTTERY (彩票调度)
- **SJF** (短作业优先，EWMA 预估)
- **SRTF** (最短剩余时间优先)
- **MLFQ** (多级反馈队列)
- **CFS** (完全公平调度器)

**内存管理：**

- 伙伴系统内存分配
- Copy-on-Write (COW)
- 懒分配
- mmap 系统调用

### 模块二：软件工程测试框架

设计了完整的测试框架，确保实现的正确性：

- 68 个测试用例
- 调度器测试（FCFS、PRIORITY、SML、LOTTERY、SJF、SRTF、MLFQ、CFS）
- 内存管理测试（伙伴系统、COW、懒分配、mmap）
- 集成测试、边界测试、压力测试
- 100% 测试通过率

### 模块三：实验数据收集和分析

**实验设计：**

- 内存管理对比实验（baseline vs testing）
- 调度器性能对比实验
- 内存测试 + 调度器测试

**数据收集：**

- perftest.c - 内存管理性能测试
- schedtest.c - 调度器性能测试

**生成图表:**

- webui/plot\_figures.py - 生成图表

### 模块四：Web 展示界面

**技术栈：**

- HTML + CSS + 原生 JavaScript
- ECharts 图表库

**页面：**

- Dashboard - 系统概览
- Scheduler - 调度器对比
- Memory - 内存管理对比

***

## 构建和运行命令

```bash
# 清理并构建
make clean && make qemu

# 生成 compile_commands.json (用于 VSCode IntelliSense)
bear -- make clean && bear -- make

# 运行 xv6 (QEMU)
make qemu

# 指定 CPU 数量运行
CPUS=1 make qemu
```

***

## 测试命令

### 模块一测试

| 测试程序        | 功能          | 运行方式        |
| ----------- | ----------- | ----------- |
| `test_sjf`  | SJF 调度器测试   | `test_sjf`  |
| `test_srtf` | SRTF 调度器测试  | `test_srtf` |
| `test_mlfq` | MLFQ 调度器测试  | `test_mlfq` |
| `test_cfs`  | CFS 调度器测试   | `test_cfs`  |
| `mmap_test` | mmap 系统调用测试 | `mmap_test` |
| `forktest`  | fork 系统调用测试 | `forktest`  |

### 模块二测试

| 测试程序             | 功能            | 运行方式             |
| ---------------- | ------------- | ---------------- |
| `test_runner`    | 运行所有单元测试 (68) | `test_runner`    |
| `scheduler_test` | 调度器功能测试       | `scheduler_test` |
| `usertests`      | 综合用户态测试       | `usertests`      |

### 模块三测试

| 测试程序        | 功能       | 运行方式        |
| ----------- | -------- | ----------- |
| `perftest`  | 内存管理性能测试 | `perftest`  |
| `schedtest` | 调度器性能测试  | `schedtest` |

### 监控工具

| 工具        | 功能     | 运行方式      |
| --------- | ------ | --------- |
| `ps`      | 进程状态监控 | `ps`      |
| `memstat` | 内存统计信息 | `memstat` |

***

## 数据分析流程

### 完整数据流架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                         完整数据流架构                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ 模块1: xv6 内核（已完成）                                     │  │
│  ├──────────────────────────────────────────────────────────────┤  │
│  │ • 9种调度算法 + 4种内存优化                                   │  │
│  │ • 输出：系统调用接口（getsnapshot, exportstats）              │  │
│  └────────────────────┬─────────────────────────────────────────┘  │
│                       │                                             │
│  ┌────────────────────▼─────────────────────────────────────────┐  │
│  │ 模块2: 测试框架                                               │  │
│  ├──────────────────────────────────────────────────────────────┤  │
│  │ • perftest.c - 内存管理测试                                   │  │
│  │ • schedtest.c - 调度器测试                                    │  │
│  │ • 输出：结构化日志文件                                        │  │
│  │   ├── webui/data/baseline.log  （baseline分支）               │  │
│  │   ├── webui/data/testing.log   （testing分支）                │  │
│  │   └── webui/data/schedtest.log （调度器测试）                 │  │
│  └────────────────────┬─────────────────────────────────────────┘  │
│                       │                                             │
│  ┌────────────────────▼─────────────────────────────────────────┐  │
│  │ 模块3: 数据分析                                               │  │
│  ├──────────────────────────────────────────────────────────────┤  │
│  │ Step 1: parse_results.py - 解析日志 → JSON                   │  │
│  │ Step 2: plot_figures.py   - 生成图表                         │  │
│  │ Step 3: generate_report.py - 生成报告                        │  │
│  │                                                               │  │
│  │ 输出：                                                        │  │
│  │ ├── webui/data/results.json      （分析结果）                  │  │
│  │ ├── webui/figures/fig1-3.png     （内存管理对比图）            │  │
│  │ └── webui/reports/experiment_report.md （实验报告）            │  │
│  └────────────────────┬─────────────────────────────────────────┘  │
│                       │                                             │
│  ┌────────────────────▼─────────────────────────────────────────┐  │
│  │ 模块4: Web展示界面                                            │  │
│  ├──────────────────────────────────────────────────────────────┤  │
│  │ 技术栈：HTML + CSS + 原生JS + ECharts                         │  │
│  │                                                               │  │
│  │ 页面：                                                        │  │
│  │ ├── index.html    - 主页面                                   │  │
│  │ ├── Dashboard     - 系统概览                                 │  │
│  │ ├── Scheduler     - 调度器对比                               │  │
│  │ └── Memory        - 内存管理对比                             │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 实验设计

#### 内存管理测试（8个）

| 测试ID        | 场景         | baseline行为 | testing行为 | 预期提升     |
| ----------- | ---------- | ---------- | --------- | -------- |
| **COW-1**   | fork+立即退出  | 复制全部5MB    | 不复制       | ↑70%     |
| **COW-2**   | fork+只读    | 复制全部5MB    | 共享页       | ↑70%     |
| **COW-3**   | fork+写30%  | 复制全部5MB    | 复制30%     | ↑40%     |
| **COW-4**   | fork+全写    | 复制全部5MB    | 缺页开销      | ↓-30% 反例 |
| **LAZY-1**  | sbrk+访问1%  | 立即分配256页   | 按需分配3页    | ↑80%     |
| **LAZY-2**  | sbrk+访问50% | 立即分配256页   | 按需分配128页  | ↑40%     |
| **LAZY-3**  | sbrk+全访问   | 立即分配256页   | 缺页风暴      | ↓-60% 反例 |
| **BUDDY-1** | 碎片测试       | 链表无法合并     | 伙伴系统合并    | 碎片率↓50%  |

#### 调度器测试（5个）

| 测试ID        | 场景    | 对比调度器              | 测量指标     |
| ----------- | ----- | ------------------ | -------- |
| **SCHED-1** | 批处理吞吐 | RR vs FCFS vs SJF  | 总完成时间    |
| **SCHED-2** | 护航效应  | RR vs FCFS vs SRTF | 短任务周转时间  |
| **SCHED-3** | 公平性   | RR vs CFS          | CPU占比标准差 |
| **SCHED-4** | 交互响应  | RR vs MLFQ         | 高优先级响应时间 |
| **SCHED-5** | 综合对比  | 全部9种调度器            | 多维度评分    |

### 使用方法

```bash
# 一键运行完整分析
make analyze

# 分步执行
./webui/collect_data.sh baseline   # 收集 baseline 数据
./webui/collect_data.sh testing    # 收集 testing 数据
python3 webui/parse_results.py     # 解析日志
python3 webui/plot_figures.py      # 生成图表
python3 webui/generate_report.py   # 生成报告

# 启动 Web 服务器
cd webui && python3 -m http.server 8080
```

***

## 文档结构

```
docs/
├── README.md                        # 文档目录说明
├── 开发进度.md                       # 项目开发进度记录
├── architecture/                    # 架构设计文档
│   ├── 1.调度算法优化.md             # 调度算法架构设计
│   ├── 2.内存管理优化.md             # 内存管理架构设计
│   ├── 3.软件工程测试框架.md         # 测试框架架构设计
│   ├── 4.数据收集与分析.md           # 数据收集分析架构设计
│   └── 5.Web展示界面.md              # Web 界面架构设计
└── log/                             # Bug 修复日志
    ├── README.md                    # Bug 日志索引
    └── BUG-*.md                     # 各个 Bug 的修复记录
```

***

## 开发进度

### ✅ 模块一：调度算法优化 + 内存管理优化

**调度算法（已完成）：**

| 调度器      | 系统调用号             | 策略                            | 状态 |
| -------- | ----------------- | ----------------------------- | -- |
| DEFAULT  | `setscheduler(0)` | Round-Robin                   | ✅  |
| FCFS     | `setscheduler(1)` | First-Come-First-Served       | ✅  |
| PRIORITY | `setscheduler(2)` | Priority (1-20)               | ✅  |
| SML      | `setscheduler(3)` | Multilevel Queue (3 级)        | ✅  |
| LOTTERY  | `setscheduler(4)` | Lottery Scheduling            | ✅  |
| **SJF**  | `setscheduler(5)` | Shortest Job First (EWMA)     | ✅  |
| **SRTF** | `setscheduler(6)` | Shortest Remaining Time First | ✅  |
| **MLFQ** | `setscheduler(7)` | Multi-Level Feedback Queue    | ✅  |
| **CFS**  | `setscheduler(8)` | Completely Fair Scheduler     | ✅  |

**内存管理（已完成）：**

| 功能                  | 测试验证          | 状态 |
| ------------------- | ------------- | -- |
| Copy-on-Write (COW) | test\_cow\.c  | ✅  |
| mmap 系统调用           | test\_mmap.c  | ✅  |
| 懒分配                 | test\_lazy.c  | ✅  |
| 伙伴系统算法              | test\_buddy.c | ✅  |

### ✅ 模块二：软件工程测试框架

**测试框架（已完成）：**

| 测试类别            | 用例数    | 通过率      | 状态 |
| --------------- | ------ | -------- | -- |
| Buddy System    | 11     | 100%     | ✅  |
| mmap            | 7      | 100%     | ✅  |
| COW             | 6      | 100%     | ✅  |
| Lazy Allocation | 10     | 100%     | ✅  |
| Integration     | 10     | 100%     | ✅  |
| Boundary        | 6      | 100%     | ✅  |
| Scheduler       | 18     | 100%     | ✅  |
| **总计**          | **68** | **100%** | ✅  |

### 🚧 模块三：实验数据分析与可视化

**已完成：**

- [x] webui/collect\_data.sh - 数据收集脚本
- [x] webui/parse\_results.py - 日志解析
- [x] webui/plot\_figures.py - 图表生成
- [x] webui/generate\_report.py - 报告生成
- [x] webui/config.yaml - 配置文件

**待完成：**

- [ ] schedtest.c - 调度器性能测试程序
- [ ] 调度器对比图表
- [ ] Web 展示界面集成

### 🚧 模块四：Web 展示界面

**待完成：**

- [ ] index.html - 主页面
- [ ] css/style.css - 样式文件
- [ ] js/app.js - 主逻辑
- [ ] js/charts.js - 图表配置
- [ ] 数据可视化集成

***

## 系统调用扩展

| 系统调用号 | 系统调用           | 功能          |
| ----- | -------------- | ----------- |
| 22    | `setscheduler` | 切换调度算法      |
| 23    | `getscheduler` | 获取当前调度算法    |
| 24    | `mmap`         | 内存映射        |
| 25    | `munmap`       | 解除映射        |
| 26    | `getptable`    | 获取进程表快照     |
| 27    | `getmemstat`   | 获取内存统计      |
| 28    | `setpriority`  | 设置进程优先级     |
| 29    | `getstats`     | 获取 CPU 统计信息 |
| 30    | `getsnapshot`  | 获取系统快照      |

***

## 核心开发原则

### 1. 测试驱动开发（TDD）

**TDD 流程：** `Red → Green → Refactor`

### 2. 数据驱动的优化证明

- 每个优化都需要量化指标
- 设计 Best Case 和 Worst Case 对比实验
- 诚实展示 Trade-off

### 3. Commit 原则

一个小模块开发完成并确保单元测试通过之后 commit

***

## 将 QEMU 输出保存到日志文件

```bash
# 方法1: 使用 tee 同时显示和保存
make qemu 2>&1 | tee test.log

# 方法2: 只保存到文件
make qemu > test.log 2>&1
```

测试完成后在 xv6 中运行的程序输出的日志就保存在项目的 test.log 中。
