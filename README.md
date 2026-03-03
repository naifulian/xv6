# xv6-riscv 操作系统扩展实现

本项目是对 MIT xv6-riscv 操作系统的深度扩展，实现了多种高级操作系统特性，包括多调度算法、伙伴系统内存分配、Copy-on-Write、懒分配和 mmap 系统调用。

## 功能特性

### 调度器模块
实现了 5 种进程调度算法，支持运行时动态切换：

| 调度器 | 算法 | 特点 | 适用场景 |
|--------|------|------|---------|
| DEFAULT | Round-Robin | 时间片轮转，公平调度 | 通用场景 |
| FCFS | 先来先服务 | 非抢占式，简单高效 | 批处理系统 |
| PRIORITY | 优先级调度 | 抢占式，优先级 1-20 | 实时系统 |
| SML | 静态多级队列 | 三级队列，不同优先级 | 混合负载 |
| LOTTERY | 彩票调度 | 概率分配，随机公平 | 多用户系统 |

### 内存管理模块

| 特性 | 描述 |
|------|------|
| **伙伴系统** | 多级空闲链表，自动合并碎片，最大支持 4MB 块 |
| **Copy-on-Write** | fork 时共享页面，写入时复制，节省内存 |
| **懒分配** | 按需分配物理页面，延迟内存分配 |
| **mmap** | 匿名私有内存映射支持 |

### 系统调用扩展

| 系统调用 | 功能 |
|---------|------|
| `setscheduler(int)` | 切换调度算法 |
| `getscheduler()` | 获取当前调度算法 |
| `mmap(...)` | 内存映射 |
| `munmap(...)` | 解除映射 |
| `setpriority(int, int)` | 设置进程优先级 |

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
make qemu-nox
```

### 测试

```bash
$ test_runner          # 运行所有 60 个测试
$ perftest              # 运行性能测试
$ ps                   # 进程状态
$ memstat              # 内存统计
```

## 项目结构

```
xv6/
├── kernel/                    # 内核源代码
│   ├── sched/                # 调度器模块
│   ├── kalloc.c              # 伙伴系统分配器
│   ├── vm.c                  # 虚拟内存 (COW, mmap, 懒分配)
│   └── trap.c                # 中断/异常处理
├── user/                     # 用户态程序
│   ├── perftest.c             # 性能测试程序
│   └── scheduler_test.c      # 调度器测试
├── tests/                     # TDD 测试框架 (60 个测试用例)
├── docs/                     # 文档目录
└── Makefile                  # 构建系统
```

## 环境要求

- QEMU >= 7.2
- RISC-V 工具链 (riscv64-unknown-elf-gcc)

## 许可证

MIT License
