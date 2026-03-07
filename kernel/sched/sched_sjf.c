// SJF (Shortest Job First) scheduler
// Non-preemptive scheduler that selects the process with shortest estimated run time
// Uses EWMA (Exponential Weighted Moving Average) for time estimation

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sched/sched.h"
#include "kernel/defs.h"

#define DEFAULT_ESTIMATED_TIME  100

static int estimated_time[NPROC];
static int actual_time[NPROC];
static int burst_count[NPROC];

static void
sjf_proc_init(struct proc *p)
{
  if(p == 0) {
    for(int i = 0; i < NPROC; i++) {
      estimated_time[i] = DEFAULT_ESTIMATED_TIME;
      actual_time[i] = 0;
      burst_count[i] = 0;
    }
    return;
  }
  
  int idx = p - proc;
  estimated_time[idx] = DEFAULT_ESTIMATED_TIME;
  actual_time[idx] = 0;
  burst_count[idx] = 0;
}

static void
sjf_update_estimate(struct proc *p, int runtime)
{
  if(p == 0) return;
  
  int idx = p - proc;
  actual_time[idx] += runtime;
  burst_count[idx]++;
  
  if(burst_count[idx] > 0) {
    int avg_time = actual_time[idx] / burst_count[idx];
    estimated_time[idx] = (estimated_time[idx] + avg_time) / 2;
  }
  
  actual_time[idx] = 0;
}

static struct proc*
sjf_pick_next(void)
{
  struct proc *p;
  struct proc *selected = 0;
  struct cpu *c = mycpu();
  int min_time = 0x7FFFFFFF;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == RUNNABLE) {
      int idx = p - proc;
      if(estimated_time[idx] < min_time) {
        if(selected != 0 && selected != p) {
          release(&selected->lock);
        }
        selected = p;
        min_time = estimated_time[idx];
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
sjf_proc_exit(struct proc *p)
{
  if(p == 0) return;
  sjf_update_estimate(p, p->rutime);
}

struct sched_ops sjf_ops = {
  .name = "SJF",
  .proc_init = sjf_proc_init,
  .pick_next = sjf_pick_next,
  .proc_exit = sjf_proc_exit,
};
