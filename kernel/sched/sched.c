// Core scheduler module for xv6-riscv
// Supports runtime dynamic switching between scheduling algorithms

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sched/sched.h"
#include "kernel/defs.h"

static struct spinlock sched_lock;

enum sched_policy current_policy = SCHED_DEFAULT;
struct sched_ops *current_ops = 0;

extern struct sched_ops default_ops;
extern struct sched_ops fcfs_ops;
extern struct sched_ops priority_ops;
extern struct sched_ops sml_ops;
extern struct sched_ops lottery_ops;
extern struct sched_ops sjf_ops;
extern struct sched_ops srtf_ops;
extern struct sched_ops mlfq_ops;
extern struct sched_ops cfs_ops;

static struct sched_ops* sched_ops_table[SCHED_CFS + 1] = {
    [SCHED_DEFAULT]  = &default_ops,
    [SCHED_FCFS]     = &fcfs_ops,
    [SCHED_PRIORITY] = &priority_ops,
    [SCHED_SML]      = &sml_ops,
    [SCHED_LOTTERY]  = &lottery_ops,
    [SCHED_SJF]      = &sjf_ops,
    [SCHED_SRTF]     = &srtf_ops,
    [SCHED_MLFQ]     = &mlfq_ops,
    [SCHED_CFS]      = &cfs_ops,
};

static void
sched_init_policy_state(struct sched_ops *ops)
{
  if(ops == 0 || ops->proc_init == 0)
    return;

  ops->proc_init(0);
  for(struct proc *p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    int active = p->state != UNUSED;
    release(&p->lock);
    if(active)
      ops->proc_init(p);
  }
}

void
sched_init(void)
{
  initlock(&sched_lock, "sched");
  current_policy = SCHED_DEFAULT;
  current_ops = &default_ops;
  sched_init_policy_state(current_ops);
}

struct proc*
sched_pick_next(void)
{
  struct sched_ops *ops = current_ops;
  if(ops && ops->pick_next)
    return ops->pick_next();
  panic("sched_pick_next: no scheduler operations");
}

int
sched_set_policy(int policy)
{
  struct sched_ops *new_ops;

  if(policy < 0 || policy > SCHED_CFS)
    return -1;
  new_ops = sched_ops_table[policy];
  if(new_ops == 0)
    return -1;

  sched_init_policy_state(new_ops);

  acquire(&sched_lock);
  current_policy = policy;
  current_ops = new_ops;
  release(&sched_lock);
  return 0;
}

void
sched_proc_init_hook(struct proc *p)
{
  struct sched_ops *ops = current_ops;
  if(ops && ops->proc_init)
    ops->proc_init(p);
}

void
sched_proc_exit_hook(struct proc *p)
{
  struct sched_ops *ops = current_ops;
  if(ops && ops->proc_exit)
    ops->proc_exit(p);
}

void
sched_proc_tick_hook(struct proc *p)
{
  struct sched_ops *ops = current_ops;
  if(ops && ops->proc_tick)
    ops->proc_tick(p);
}

void
sched_proc_wakeup_hook(struct proc *p)
{
  struct sched_ops *ops = current_ops;
  if(ops && ops->proc_wakeup)
    ops->proc_wakeup(p);
}

const char*
sched_policy_name(int policy)
{
  switch(policy) {
    case SCHED_DEFAULT:
      return "DEFAULT";
    case SCHED_FCFS:
      return "FCFS";
    case SCHED_PRIORITY:
      return "PRIORITY";
    case SCHED_SML:
      return "SML";
    case SCHED_LOTTERY:
      return "LOTTERY";
    case SCHED_SJF:
      return "SJF";
    case SCHED_SRTF:
      return "SRTF";
    case SCHED_MLFQ:
      return "MLFQ";
    case SCHED_CFS:
      return "CFS";
    default:
      return "UNKNOWN";
  }
}
