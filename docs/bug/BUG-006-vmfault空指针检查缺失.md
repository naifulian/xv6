# BUG-006: vmfault 空指针检查缺失

## 基本信息

| 项目 | 内容 |
|------|------|
| 编号 | BUG-006 |
| 分类 | 内存模块 (MEM) |
| 严重程度 | 高 |
| 发现时间 | 2026-02-15 |
| 修复提交 | `1d240ac`, `d931fce` |
| 影响文件 | `kernel/vm.c` |

## 问题描述

`vmfault` 函数没有检查地址是否小于 `PGSIZE`，可能导致空指针访问。

## 问题代码

```c
uint64 vmfault(pagetable_t pagetable, uint64 va, int read)
{
    // 缺少 va < PGSIZE 检查
    if(va >= p->sz)
        return 0;
    // ...
}
```

## 问题分析

1. **安全漏洞**：地址 0 附近可能被错误访问
2. **与 guard page 冲突**：地址 0 到 PGSIZE 应该是无效的
3. **可能导致崩溃**：空指针解引用

## 影响范围

- 懒分配可能错误地分配 guard page 区域
- 可能导致系统崩溃
- 安全隐患

## 修复方案

添加地址下界检查。

## 修复后代码

```c
uint64 vmfault(pagetable_t pagetable, uint64 va, int read)
{
    if(va < PGSIZE || va >= p->sz)  // 添加 va < PGSIZE 检查
        return 0;
    // ...
}
```

## 反省

1. **边界检查不完整**：只检查了上界，没有检查下界
2. **安全意识不足**：没有考虑空指针场景
3. **改进建议**：所有地址检查都应该包含上下界

## 影响评估

- 功能影响：重大，修复后避免崩溃
- 安全性：显著提高
- 稳定性：改善
