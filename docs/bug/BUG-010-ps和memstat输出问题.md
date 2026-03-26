# BUG-010: ps 和 memstat 输出问题

## 基本信息

| 项目 | 内容 |
|------|------|
| 编号 | BUG-010 |
| 分类 | 用户工具 (USER) |
| 严重程度 | 中 |
| 发现时间 | 2026-02-15 |
| 修复提交 | `384cc17` |
| 影响文件 | `kernel/kalloc.c`, `kernel/sysproc.c`, `user/printf.c`, `user/ps.c` |

## 问题描述

多个问题导致 ps 和 memstat 工具输出不正确：
1. `fill_memstat` 空闲页数计算错误
2. `pstat` 结构体类型不一致
3. `printf` 不支持格式化宽度
4. ps 输出格式混乱

## 问题代码

```c
// 问题1: 空闲页数计算错误
void fill_memstat(struct memstat *ms)
{
    ms->free_pages = ???;  // 没有正确计算
}

// 问题2: 类型不一致
// kernel/sysproc.c
p->sz  // int 类型

// 用户态
struct pstat {
    int sz;  // 应该是 uint
};

// 问题3: printf 不支持宽度
printf("%-4d", pid);  // 不生效
```

## 问题分析

1. **数据不准确**：内存统计信息错误
2. **类型不匹配**：内核和用户态结构体不一致
3. **输出格式差**：对齐不正确，难以阅读

## 影响范围

- 监控工具输出不可用
- 性能分析数据不准确
- 用户体验差

## 修复方案

1. 正确计算空闲页数
2. 统一类型定义
3. 添加 printf 格式化宽度支持
4. 简化 ps 输出逻辑

## 修复后代码

```c
// 修复1: 正确计算空闲页数
void fill_memstat(struct memstat *ms)
{
    uint free_count = 0;
    for(int order = 0; order < MAX_ORDER; order++) {
        struct run *r = kmem.free_lists[order].head;
        while(r) {
            free_count += (1 << order);
            r = r->next;
        }
    }
    ms->free_pages = free_count;
}

// 修复2: 类型一致
struct pstat {
    uint sz;  // 改为 uint
};

// 修复3: printf 支持宽度
// 添加 %-Nd 格式支持
```

## 反省

1. **测试不充分**：没有实际运行验证输出
2. **类型一致性检查不足**：内核和用户态结构体应该同步
3. **改进建议**：添加工具测试用例

## 影响评估

- 功能影响：重大，修复后工具可用
- 数据准确性：显著提高
- 用户体验：改善
