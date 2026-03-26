# xv6-riscv 操作系统扩展实现

本项目是对 MIT xv6-riscv 操作系统的深度扩展，实现了多种高级操作系统特性，包括多调度算法、伙伴系统内存分配、Copy-on-Write、懒分配和 mmap 系统调用。

## 项目架构

本项目分为四个主要模块：

### 模块一：调度算法优化 + 内存管理优化

#### 调度算法架构

**设计理念：**
- 模块化设计：每个调度器独立实现，易于扩展
- 运行时切换：支持动态切换调度算法
- 避免修改核心数据结构：使用独立数组存储调度器数据

**架构设计：**

```
kernel/sched/
├── sched.c              # 调度器核心框架
│   ├── sched_init()     # 初始化调度器
│   ├── sched_pick_next() # 选择下一个进程
│   ├── sched_set_policy() # 切换调度算法
│   └── sched_policy_name() # 获取调度器名称
├── sched_default.c      # Round-Robin 调度器
├── sched_fcfs.c         # FCFS 调度器
├── sched_priority.c     # 优先级调度器
├── sched_sml.c          # 静态多级队列调度器
├── sched_lottery.c      # 彩票调度器
├── sched_sjf.c          # SJF 调度器（EWMA 预估）
├── sched_srtf.c         # SRTF 调度器（抢占式）
├── sched_mlfq.c         # MLFQ 调度器（动态优先级）
├── sched_cfs.c          # CFS 调度器（vruntime）
└── sched_stats.c        # CPU 统计模块
```

**调度器特性：**

| 调度器 | 算法 | 特点 | 适用场景 |
|--------|------|------|---------|
| DEFAULT | Round-Robin | 时间片轮转，公平调度 | 通用场景 |
| FCFS | 先来先服务 | 非抢占式，简单高效 | 批处理系统 |
| PRIORITY | 优先级调度 | 抢占式，优先级 1-20 | 实时系统 |
| SML | 静态多级队列 | 三级队列，不同优先级 | 混合负载 |
| LOTTERY | 彩票调度 | 概率分配，随机公平 | 多用户系统 |
| **SJF** | 短作业优先 | EWMA 时间预估，非抢占式 | 批处理系统 |
| **SRTF** | 最短剩余时间优先 | 抢占式 SJF，动态调整 | 交互系统 |
| **MLFQ** | 多级反馈队列 | 3级队列，动态优先级调整 | 通用场景 |
| **CFS** | 完全公平调度器 | vruntime 公平调度，类 Linux | 通用场景 |

**技术亮点：**
- EWMA（指数加权移动平均）时间预估机制
- 动态优先级调整算法
- vruntime 公平调度实现
- 模块化设计，易于扩展

#### 内存管理架构

**设计理念：**
- 伙伴系统：多级空闲链表，自动合并碎片
- COW：写时复制，节省内存
- 懒分配：按需分配，延迟内存分配
- mmap：匿名私有内存映射

**架构设计：**

```
kernel/
├── kalloc.c             # 伙伴系统内存分配器
│   ├── kalloc()         # 分配页面
│   ├── kfree()          # 释放页面
│   └── buddy system     # 伙伴系统实现
├── vm.c                 # 虚拟内存管理
│   ├── uvmcopy()        # COW 实现
│   ├── uvmunmap()       # 解除映射
│   ├── mmap()           # mmap 系统调用
│   └── lazy allocation  # 懒分配实现
└── trap.c               # 中断/异常处理
    └── page fault handler # 页错误处理（懒分配、COW）
```

**性能提升：**
- COW fork 不访问：↑94%
- COW fork 只读：↑94%
- 懒分配访问 1%：↑100%

### 模块二：软件工程测试框架

#### 测试框架架构

**设计理念：**
- TDD（测试驱动开发）：Red → Green → Refactor
- 模块化测试：每个功能独立测试
- 自动化测试：一键运行所有测试

**架构设计：**

```
tests/
├── include/              # 测试头文件
│   ├── test_common.h     # 通用测试框架
│   ├── test_scheduler.h  # 调度器测试
│   ├── test_buddy.h      # 伙伴系统测试
│   ├── test_cow.h        # COW 测试
│   ├── test_lazy.h       # 懒分配测试
│   └── test_mmap.h       # mmap 测试
└── src/                  # 测试实现
    ├── test_runner.c     # 测试运行器
    ├── test_scheduler.c  # 调度器测试实现
    ├── test_buddy.c      # 伙伴系统测试实现
    └── ...
```

**测试覆盖：**

| 测试类别 | 用例数 | 通过率 | 状态 |
|----------|--------|--------|------|
| Buddy System | 11 | 100% | ✅ |
| mmap | 6 | 100% | ✅ |
| COW | 6 | 100% | ✅ |
| Lazy Allocation | 10 | 100% | ✅ |
| Integration | 10 | 100% | ✅ |
| Boundary | 6 | 100% | ✅ |
| Scheduler (旧) | 10 | 100% | ✅ |
| **Scheduler (新)** | 待添加 | - | 🚧 |

### 模块三：实验数据收集与分析

#### 数据收集架构

**设计理念：**
- baseline / testing 多轮对比
- memory / scheduler 分离 boot 采集
- 自动校验 + 自动出图 + 论文产物导出

**架构设计：**

```
实验链路：
user/perftest.c
  -> META / RESULT / SAMPLES
  -> experiments/collect/collect_data.sh
  -> experiments/analyze/experiment_data.py
  -> experiments/analyze/validate_logs.py
  -> experiments/analyze/plot_results.py
  -> experiments/outputs/data/*.json / *.csv / experiments/outputs/figures/*.png
```

**当前能力：**

- 多轮 round-level 聚合
- raw sample-level 数据保留
- partial run manifest 审计
- Markdown / CSV / LaTeX 论文产物导出
- 默认日志根目录已迁移为 `experiments/logs/`
- 详见 [模块三索引](/home/niya/xv6/docs/module3/README.md) 与 [文档索引](/home/niya/xv6/docs/README.md)

### 模块四：Web 展示界面

#### Web 界面架构

**设计理念：**
- 纯前端实现：HTML + CSS + JavaScript
- ECharts 图表：数据可视化
- Python HTTP 服务器：简单部署

**架构设计：**

```
dashboard/
├── index.html           # 主页面
├── css/
│   └── style.css       # 样式
├── js/
│   ├── app.js          # 主逻辑
│   ├── charts.js       # 图表配置
│   └── data.js         # 数据加载
├── data/
│   ├── scheduler.json  # 调度器数据
│   ├── memory.json     # 内存数据
│   └── performance.json # 性能对比数据
└── data/
    ├── snapshots.json  # 运行态快照
    └── sysmon.raw      # 原始监控输出
```

**技术栈：**
- 前端：HTML5 + CSS3 + 原生 JavaScript
- 图表：ECharts 5.x（CDN 或本地化）
- 服务器：`python -m http.server`

**页面功能：**
1. 调度器对比页面：9种调度器性能对比
2. 内存管理页面：内存使用统计
3. 性能分析页面：性能对比图表

## 系统调用扩展

| 系统调用号 | 系统调用 | 功能 |
|-----------|---------|------|
| 22 | `setscheduler` | 切换调度算法 |
| 23 | `getscheduler` | 获取当前调度算法 |
| 24 | `mmap` | 内存映射 |
| 25 | `munmap` | 解除映射 |
| 26 | `getptable` | 获取进程表快照 |
| 27 | `getmemstat` | 获取内存统计 |
| 28 | `setpriority` | 设置进程优先级 |
| 29 | `getstats` | 获取 CPU 统计信息 |
| 30 | `getsnapshot` | 获取系统快照 |

## 快速开始

### 构建

```bash
make clean && make
```

### 运行

```bash
make qemu
```

### 测试

```bash
$ test_runner          # 运行所有测试
$ perftest              # 性能测试
$ ps                   # 进程状态
$ memstat              # 内存统计
$ test_sjf             # SJF 测试
$ test_srtf            # SRTF 测试
$ test_mlfq            # MLFQ 测试
$ test_cfs             # CFS 测试
```

## 项目结构

```
xv6/
├── kernel/                    # 内核源代码
│   ├── sched/                # 调度器模块
│   │   ├── sched.c           # 调度器核心
│   │   ├── sched_default.c   # Round-Robin
│   │   ├── sched_fcfs.c      # FCFS
│   │   ├── sched_priority.c  # 优先级调度
│   │   ├── sched_sml.c       # 静态多级队列
│   │   ├── sched_lottery.c   # 彩票调度
│   │   ├── sched_sjf.c       # SJF
│   │   ├── sched_srtf.c      # SRTF
│   │   ├── sched_mlfq.c      # MLFQ
│   │   ├── sched_cfs.c       # CFS
│   │   └── sched_stats.c     # CPU 统计
│   ├── sysinfo.c             # 系统快照
│   ├── kalloc.c              # 伙伴系统分配器
│   ├── vm.c                  # 虚拟内存 (COW, mmap, 懒分配)
│   └── trap.c                # 中断/异常处理
├── user/                     # 用户态程序
│   ├── test_runner.c         # 测试运行器
│   ├── perftest.c            # 性能测试
│   └── ...                   # 其他测试程序
├── tests/                    # 测试框架
│   ├── include/              # 测试头文件
│   └── src/                  # 测试实现
├── docs/                     # 文档目录
│   ├── review/               # 审查报告
│   ├── bug/                  # Bug 档案
│   ├── module1-4/            # 模块文档
│   └── 开发进度.md           # 开发进度
└── Makefile                  # 构建系统
```

## 开发进度

### ✅ 模块一：调度算法优化 + 内存管理优化

- [x] 9种调度器实现
- [x] 内存管理优化
- [x] 所有功能测试通过

### ✅ 模块二：软件工程测试框架

- [x] 测试框架设计
- [x] 60+ 测试用例
- [x] 100% 测试通过率

### 🚧 模块三：实验数据收集与分析

- [x] 数据收集机制
- [x] Python 分析脚本
- [x] 图表生成
- [x] 多轮校验与报告导出

### 🚧 模块四：Web 展示界面

- [x] 设计方案
- [ ] Web 界面实现
- [ ] 数据可视化

## 技术亮点

1. **模块化设计**：每个调度器独立实现，易于扩展和维护
2. **避免修改核心数据结构**：使用独立数组存储调度器数据
3. **完整的测试框架**：60+ 测试用例，确保实现正确性
4. **类 Linux 实现**：CFS 调度器参考真实系统
5. **性能优化**：COW 和懒分配显著提升性能

## 环境要求

- QEMU >= 7.2
- RISC-V 工具链 (riscv64-unknown-elf-gcc)
- Python >= 3.8（数据分析和可视化）

## 许可证

MIT License
