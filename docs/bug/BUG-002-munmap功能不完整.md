# BUG-002: munmap 系统调用功能不完整

## 基本信息

| 项目 | 内容 |
|------|------|
| 编号 | BUG-002 |
| 分类 | 内存模块 (MEM) |
| 严重程度 | 高 |
| 发现时间 | 2026-03-05 |
| 修复提交 | `8fa0c4e` |
| 影响文件 | `kernel/sysproc.c` |

## 问题描述

`munmap` 系统调用只支持从地址空间末尾取消映射，不支持释放中间的映射区域，不符合 POSIX 标准。

## 问题代码

```c
uint64 sys_munmap(void)
{
    uint64 addr, length;
    struct proc *p = myproc();

    argaddr(0, &addr);
    argaddr(1, &length);

    if(length == 0)
        return 0;

    addr = PGROUNDDOWN(addr);
    length = PGROUNDUP(length);

    if(addr >= p->sz)
        return -1;

    // 只支持从地址空间末尾取消映射
    if(addr + length != p->sz)
        return -1;  // 拒绝非末尾的取消映射

    p->sz = addr;
    uvmdealloc(p->pagetable, addr + length, addr);
    return 0;
}
```

## 问题分析

1. **功能限制**：无法释放地址空间中间的映射
2. **不符合标准**：POSIX mmap 规范要求支持任意位置取消映射
3. **实用性受限**：限制了 mmap 的使用场景

## 影响范围

- 无法实现内存池的局部释放
- 无法支持内存映射文件的随机释放
- 影响 mmap 测试用例的完整性

## 修复方案

支持任意位置的取消映射，同时正确处理地址空间大小的更新。

## 修复后代码

```c
uint64 sys_munmap(void)
{
    uint64 addr, length;
    struct proc *p = myproc();

    argaddr(0, &addr);
    argaddr(1, &length);

    if(length == 0)
        return 0;

    addr = PGROUNDDOWN(addr);
    length = PGROUNDUP(length);

    if(addr >= p->sz || addr + length > p->sz)
        return -1;

    uvmunmap(p->pagetable, addr, length / PGSIZE, 1);

    if(addr + length == p->sz)
        p->sz = addr;

    return 0;
}
```

## 反省

1. **设计不完整**：最初实现时只考虑了简单场景
2. **标准参考不足**：没有充分参考 POSIX 规范
3. **测试用例缺失**：没有设计测试中间释放的场景
4. **改进建议**：实现新功能时应先研究标准规范

## 影响评估

- 功能影响：重大，修复后功能完整
- 性能影响：无
- 兼容性：提高 POSIX 兼容性
