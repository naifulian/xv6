// CFS (Completely Fair Scheduler) scheduler
// Linux's default scheduler - aims for fair CPU time distribution
// Uses virtual runtime (vruntime) to track process execution
// Lower vruntime = higher priority for scheduling

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sched/sched.h"
#include "kernel/defs.h"

#define DEFAULT_WEIGHT       1024
#define MIN_VRUNTIME         0

static uint64 vruntime[NPROC];
static int weight[NPROC];
static uint64 min_vruntime;

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
  vruntime[idx] = min_vruntime;
  weight[idx] = DEFAULT_WEIGHT;
}

static void
cfs_update_vruntime(struct proc *p, int runtime)
{
  if(p == 0) return;
  
  int idx = p - proc;
  uint64 delta = (runtime * DEFAULT_WEIGHT) / weight[idx];
  vruntime[idx] += delta;
  
  if(vruntime[idx] < min_vruntime) {
    min_vruntime = vruntime[idx];
  }
}

static struct proc*
cfs_pick_next(void)
{
  struct proc *p;
  struct proc *selected = 0;
  struct cpu *c = mycpu();
  uint64 min_vr = 0xFFFFFFFFFFFFFFFFULL;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == RUNNABLE) {
      int idx = p - proc;
      if(vruntime[idx] < min_vr) {
        if(selected != 0 && selected != p) {
          release(&selected->lock);
        }
        selected = p;
        min_vr = vruntime[idx];
      } else {
        release(&p->lock);
      }
    } else {
      release(&p->lock);
    }
  }
  
  if(selected != 0) {
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
  if(p == 0) return;
  cfs_update_vruntime(p, p->rutime);
}

struct sched_ops cfs_ops = {
  .name = "CFS",
  .proc_init = cfs_proc_init,
  .pick_next = cfs_pick_next,
  .proc_exit = cfs_proc_exit,
};
