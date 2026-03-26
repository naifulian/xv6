# BUG-012: copyout 未处理 COW 页面

## 基本信息

| 项目 | 内容 |
|------|------|
| 编号 | BUG-012 |
| 分类 | 内存模块 (MEM) |
| 严重程度 | 高 |
| 发现时间 | 2026-02-15 |
| 修复提交 | `1d240ac` |
| 影响文件 | `kernel/vm.c` |

## 问题描述

`copyout()` 函数在写入 COW 页面时，检测到页面不可写直接返回错误，导致系统调用失败。

## 问题代码

```c
int copyout(pagetable_t pagetable, uint64 va, char *src, uint64 len)
{
    // ... 
    if((*pte & PTE_W) == 0) {
        return -1;  // 直接返回错误，未处理 COW
    }
    // ...
}
```

## 问题分析

1. **现象**：系统调用返回错误
2. **调试过程**：
   - 在 `copyout()` 添加打印
   - 发现写入 COW 页面时失败
3. **根本原因**：
   - COW 页面被标记为只读
   - `copyout()` 检测到不可写直接返回错误

## 影响范围

- 系统调用写入用户内存失败
- 进程间通信异常
- 文件系统操作异常

## 修复方案

在 `copyout()` 中处理 COW 页面，触发页面复制。

## 修复后代码

```c
int copyout(pagetable_t pagetable, uint64 va, char *src, uint64 len)
{
    // ...
    pte_t *pte = walk(pagetable, va, 0);
    
    // 处理 COW 页面
    if((*pte & PTE_COW) && (*pte & PTE_V)) {
        if(cow_alloc(pagetable, va) == 0) {
            return -1;
        }
        pte = walk(pagetable, va, 0);  // 重新获取 PTE
    }
    
    // ...
}
```

## 反省

1. **边界情况遗漏**：COW 页面需要特殊处理
2. **内核态访问用户内存**：需要考虑所有可能的页面状态
3. **改进建议**：所有访问用户内存的函数都需要处理 COW

## 影响评估

- 功能影响：重大，修复后系统调用正常
- COW 功能：完整实现
- 系统稳定性：提高
