# xv6-riscv 操作系统扩展实现

本项目是对 MIT xv6-riscv 操作系统的深度扩展，实现了多种高级操作系统特性，包括多调度算法、伙伴系统内存分配、Copy-on-Write、懒分配和 mmap 系统调用。

## 功能特性

### 调度器模块
实现了 9 种进程调度算法，支持运行时动态切换：

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

### 内存管理模块

| 特性 | 描述 |
|------|------|
| **伙伴系统** | 多级空闲链表，自动合并碎片，最大支持 4MB 块 |
| **Copy-on-Write** | fork 时共享页面，写入时复制，节省内存 |
| **懒分配** | 按需分配物理页面，延迟内存分配 |
| **mmap** | 匿名私有内存映射支持 |

### 系统监控模块

| 工具 | 功能 |
|------|------|
| **CPU 统计** | CPU 使用率、上下文切换、中断统计、进程创建/退出统计 |
| **系统快照** | 收集系统状态、内存统计、进程统计、调度器信息 |
| **sysmon** | 实时系统监控工具，类似 Linux top 命令 |
| **sched_demo** | 调度器性能演示程序 |

### 系统调用扩展

| 系统调用 | 功能 |
|---------|------|
| `setscheduler(int)` | 切换调度算法 |
| `getscheduler()` | 获取当前调度算法 |
| `mmap(...)` | 内存映射 |
| `munmap(...)` | 解除映射 |
| `setpriority(int, int)` | 设置进程优先级 |
| `getstats()` | 获取 CPU 统计信息 |
| `getsnapshot(void*)` | 获取系统快照 |

## 性能测试结果

### COW 测试对比

| 测试 | Baseline (Eager) | Testing (COW) | 提升 |
|------|------------------|---------------|------|
| fork 不访问 | 18 ticks | 1 tick | ↑94% |
| fork 只读 | 18 ticks | 1 tick | ↑94% |
| fork 写 30% | 18 ticks | 15 ticks | ↑17% |
| fork 全写 | 19 ticks | 42 ticks | ↓121% |

### 懒分配测试对比

| 测试 | Baseline (Eager) | Testing (Lazy) | 提升 |
|------|------------------|----------------|------|
| 访问 1% | 10 ticks | 0 tick | ↑100% |
| 访问 50% | 8 ticks | 9 ticks | ↓13% |
| 访问 100% | 9 ticks | 16 ticks | ↓78% |

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
$ test_runner          # 运行所有 60 个测试
$ perftest              # 运行性能测试
$ ps                   # 进程状态
$ memstat              # 内存统计
$ sysmon               # 实时系统监控
$ sched_demo           # 调度器演示
$ test_sjf             # SJF 调度器测试
$ test_srtf            # SRTF 调度器测试
$ test_mlfq            # MLFQ 调度器测试
$ test_cfs             # CFS 调度器测试
$ test_stats           # CPU 统计测试
$ test_snapshot        # 系统快照测试
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
│   │   ├── sched_sjf.c       # SJF (新增)
│   │   ├── sched_srtf.c      # SRTF (新增)
│   │   ├── sched_mlfq.c      # MLFQ (新增)
│   │   ├── sched_cfs.c       # CFS (新增)
│   │   └── sched_stats.c     # CPU 统计 (新增)
│   ├── sysinfo.c             # 系统快照 (新增)
│   ├── kalloc.c              # 伙伴系统分配器
│   ├── vm.c                  # 虚拟内存 (COW, mmap, 懒分配)
│   └── trap.c                # 中断/异常处理
├── user/                     # 用户态程序
│   ├── perftest.c             # 性能测试程序
│   ├── scheduler_test.c      # 调度器测试
│   ├── sysmon.c              # 系统监控工具 (新增)
│   ├── sched_demo.c          # 调度器演示 (新增)
│   ├── test_sjf.c            # SJF 测试 (新增)
│   ├── test_srtf.c           # SRTF 测试 (新增)
│   ├── test_mlfq.c           # MLFQ 测试 (新增)
│   ├── test_cfs.c            # CFS 测试 (新增)
│   ├── test_stats.c          # CPU 统计测试 (新增)
│   └── test_snapshot.c       # 系统快照测试 (新增)
├── tests/                     # TDD 测试框架 (60 个测试用例)
├── docs/                     # 文档目录
│   └── log/                  # 开发日志和 BUG 记录
└── Makefile                  # 构建系统
```

## 开发进度

### Phase 1: 新型调度器实现 ✅

- [x] Phase 1.1: SJF 调度器（含 EWMA 预估机制）
- [x] Phase 1.2: SRTF 调度器（抢占式 SJF）
- [x] Phase 1.3: MLFQ 调度器（含 I/O 判断）
- [x] Phase 2: CFS 调度器（类 Linux 实现）

### Phase 3: 监控与统计 ✅

- [x] Phase 3.1: CPU 统计功能
- [x] Phase 3.2: 系统快照数据结构
- [x] Phase 3.3: sysmon 监控工具
- [x] Phase 3.4: sched_demo 演示程序

### Phase 4: 测试与优化 ✅

- [x] 完善对比实验和测试
- [x] 所有测试通过

## 技术亮点

1. **模块化设计**：每个调度器独立实现，易于扩展和维护
2. **避免修改核心数据结构**：使用独立数组存储调度器数据，避免修改 `struct proc`
3. **完整的监控体系**：从统计收集到可视化展示，形成完整监控链
4. **丰富的测试**：每个功能都有专门的测试程序，确保质量
5. **类 Linux 实现**：CFS 调度器参考 Linux 实现，具有实际应用价值

## 环境要求

- QEMU >= 7.2
- RISC-V 工具链 (riscv64-unknown-elf-gcc)

## 许可证

MIT License
