// Scheduler interface for xv6-riscv
// Supports runtime dynamic switching between scheduling algorithms

#ifndef SCHED_H
#define SCHED_H

#include "kernel/types.h"
#include "kernel/param.h"

struct proc;

enum sched_policy {
    SCHED_DEFAULT,
    SCHED_FCFS,
    SCHED_PRIORITY,
    SCHED_SML,
    SCHED_LOTTERY,
    SCHED_SJF,
    SCHED_SRTF,
    SCHED_MLFQ,
    SCHED_CFS,
};

struct sched_ops {
    const char *name;
    void (*proc_init)(struct proc *p);
    struct proc* (*pick_next)(void);
    void (*proc_exit)(struct proc *p);
    void (*proc_tick)(struct proc *p);
    void (*proc_wakeup)(struct proc *p);
};

extern enum sched_policy current_policy;
extern struct sched_ops *current_ops;

void sched_init(void);
struct proc* sched_pick_next(void);
void sched_update_stats(void);
int sched_set_policy(int policy);
const char* sched_policy_name(int policy);
void sched_proc_init_hook(struct proc *p);
void sched_proc_exit_hook(struct proc *p);
void sched_proc_tick_hook(struct proc *p);
void sched_proc_wakeup_hook(struct proc *p);

void sched_stats_init(void);
void sched_tick(void);
void sched_idle_tick(void);
void sched_context_switch(void);
void sched_interrupt(void);
void sched_proc_created(void);
void sched_proc_exited(void);
void sched_syscall(void);
int sys_getstats_kernel(void);

#define DEFAULT_PRIORITY    10
#define DEFAULT_SML_CLASS    2
#define DEFAULT_TICKETS      1
#define MIN_PRIORITY         1
#define MAX_PRIORITY         20
#define MIN_SML_CLASS        1
#define MAX_SML_CLASS        3
#define MIN_TICKETS          1
#define MAX_TICKETS          100

#endif
