# BUG-008: exit status 处理错误

## 基本信息

| 项目 | 内容 |
|------|------|
| 编号 | BUG-008 |
| 分类 | 测试模块 (TEST) |
| 严重程度 | 中 |
| 发现时间 | 2026-02-15 |
| 修复提交 | `1d240ac` |
| 影响文件 | `tests/src/test_scheduler.c`, `tests/src/test_integration.c` |

## 问题描述

测试代码使用 POSIX 的 `WEXITSTATUS` 宏处理退出状态，但 xv6 的 exit status 是直接存储的，不需要移位。

## 问题代码

```c
// 错误：xv6 不使用 POSIX 格式
if(WEXITSTATUS(status) == expected) {
    // ...
}
```

## 问题分析

1. **API 理解错误**：xv6 的 wait() 直接返回 exit code
2. **移植问题**：直接使用 POSIX 宏导致错误
3. **测试失败**：退出状态检查不正确

## xv6 vs POSIX

| 系统 | exit status 存储方式 |
|------|----------------------|
| POSIX | status >> 8 |
| xv6 | 直接存储 |

## 修复方案

直接比较 status 值。

## 修复后代码

```c
// 正确：xv6 直接存储 exit status
if(status == expected) {
    // ...
}
```

## 反省

1. **API 文档阅读不足**：没有仔细阅读 xv6 的 wait 实现
2. **假设错误**：假设 xv6 与 POSIX 行为一致
3. **改进建议**：移植代码时需要确认 API 差异

## 影响评估

- 功能影响：测试验证错误
- 测试准确性：修复后正确
- 可移植性：提醒 xv6 与 POSIX 差异
