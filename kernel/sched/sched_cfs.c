// CFS (Completely Fair Scheduler) scheduler
// Linux's default scheduler - aims for fair CPU time distribution

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sched/sched.h"
#include "kernel/defs.h"

#define DEFAULT_WEIGHT 1024

static uint64 vruntime[NPROC];
static int weight[NPROC];
static uint64 min_vruntime;

static uint64
cfs_find_min_vruntime(int skip_idx, int *found)
{
  uint64 min = 0;

  *found = 0;
  for(struct proc *p = proc; p < &proc[NPROC]; p++) {
    int idx = p - proc;

    if(idx == skip_idx)
      continue;

    acquire(&p->lock);
    if(p->state != UNUSED && p->state != ZOMBIE) {
      if(*found == 0 || vruntime[idx] < min) {
        min = vruntime[idx];
        *found = 1;
      }
    }
    release(&p->lock);
  }

  return min;
}

static void
cfs_proc_init(struct proc *p)
{
  if(p == 0) {
    min_vruntime = 0;
    for(int i = 0; i < NPROC; i++) {
      vruntime[i] = 0;
      weight[i] = DEFAULT_WEIGHT;
    }
    return;
  }

  int idx = p - proc;
  int found;
  uint64 base = cfs_find_min_vruntime(idx, &found);

  if(found)
    min_vruntime = base;
  vruntime[idx] = min_vruntime;
  weight[idx] = DEFAULT_WEIGHT;
}

static void
cfs_update_vruntime(struct proc *p, int runtime)
{
  if(p == 0 || runtime <= 0)
    return;

  int idx = p - proc;
  uint64 delta = (uint64)runtime * DEFAULT_WEIGHT / weight[idx];
  if(delta == 0)
    delta = 1;
  vruntime[idx] += delta;
}

static struct proc*
cfs_pick_next(void)
{
  struct proc *p;
  struct proc *selected = 0;
  struct cpu *c = mycpu();
  uint64 min_vr = 0xFFFFFFFFFFFFFFFFULL;
  uint64 earliest_ctime = 0xffffffffffffffffULL;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == RUNNABLE) {
      int idx = p - proc;
      if(vruntime[idx] < min_vr ||
         (vruntime[idx] == min_vr && p->ctime < earliest_ctime)) {
        if(selected != 0 && selected != p)
          release(&selected->lock);
        selected = p;
        min_vr = vruntime[idx];
        earliest_ctime = p->ctime;
      } else {
        release(&p->lock);
      }
    } else {
      release(&p->lock);
    }
  }

  if(selected != 0) {
    min_vruntime = min_vr;
    selected->state = RUNNING;
    c->proc = selected;
    swtch(&c->context, &selected->context);
    c->proc = 0;
    release(&selected->lock);
    return selected;
  }

  return 0;
}

static void
cfs_proc_exit(struct proc *p)
{
  int idx;
  int found;

  if(p == 0)
    return;

  idx = p - proc;
  vruntime[idx] = min_vruntime;
  weight[idx] = DEFAULT_WEIGHT;

  min_vruntime = cfs_find_min_vruntime(idx, &found);
  if(found == 0)
    min_vruntime = 0;
}

static void
cfs_proc_tick(struct proc *p)
{
  cfs_update_vruntime(p, 1);
}

struct sched_ops cfs_ops = {
  .name = "CFS",
  .proc_init = cfs_proc_init,
  .pick_next = cfs_pick_next,
  .proc_exit = cfs_proc_exit,
  .proc_tick = cfs_proc_tick,
};
