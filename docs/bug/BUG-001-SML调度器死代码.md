# BUG-001: SML 调度器死代码

## 基本信息

| 项目 | 内容 |
|------|------|
| 编号 | BUG-001 |
| 分类 | 调度模块 (SCHED) |
| 严重程度 | 低 |
| 发现时间 | 2026-03-05 |
| 修复提交 | `8fa0c4e` |
| 影响文件 | `kernel/sched/sched_sml.c` |

## 问题描述

在 `sml_pick_next()` 函数中，`found` 变量被声明和设置，但从未实际使用。由于 `return p` 语句在内层循环中执行，外层的 `if(found)` 代码块成为死代码。

## 问题代码

```c
static struct proc*
sml_pick_next(void)
{
    // ...
    for(int class = 1; class <= 3; class++) {
        int start_index = rr_index[class];
        int found = 0;  // 声明了但未真正使用

        for(int i = 0; i < NPROC; i++) {
            p = &proc[(start_index + i) % NPROC];
            acquire(&p->lock);
            if(p->state == RUNNABLE && p->sched_class == class) {
                // ...
                release(&p->lock);
                return p;  // 在内层循环中返回
            }
            release(&p->lock);
        }

        // 死代码：永远不会执行到这里
        if(found) {
            return p;
        }
    }
    return 0;
}
```

## 问题分析

1. `found` 变量被设置为 1，但 `return p` 已经在内层循环执行
2. 外层的 `if(found)` 永远不会被执行
3. 这是代码质量问题，不影响功能正确性

## 修复方案

删除无用的 `found` 变量和死代码，简化函数逻辑。

## 修复后代码

```c
static struct proc*
sml_pick_next(void)
{
    struct proc *p;
    struct cpu *c = mycpu();

    for(int class = 1; class <= 3; class++) {
        int start_index = rr_index[class];

        for(int i = 0; i < NPROC; i++) {
            p = &proc[(start_index + i) % NPROC];

            acquire(&p->lock);
            if(p->state == RUNNABLE && p->sched_class == class) {
                rr_index[class] = (start_index + i + 1) % NPROC;

                p->state = RUNNING;
                c->proc = p;
                swtch(&c->context, &p->context);

                c->proc = 0;
                release(&p->lock);
                return p;
            }
            release(&p->lock);
        }
    }

    return 0;
}
```

## 反省

1. **代码审查不够严格**：开发时没有注意到这个逻辑问题
2. **测试覆盖不足**：单元测试没有检测到死代码
3. **改进建议**：使用静态分析工具检测死代码

## 影响评估

- 功能影响：无
- 性能影响：无
- 代码质量：轻微改善
