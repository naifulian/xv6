// FCFS (First-Come-First-Served) scheduler
// Selects the process with the earliest creation time (ctime)
// Non-preemptive: once a process starts, it runs until it yields or blocks

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sched/sched.h"
#include "kernel/defs.h"

// Process initialization hook for FCFS scheduler
// ctime is already set in allocproc(), so no special init needed
static void
fcfs_proc_init(struct proc *p)
{
  // No special initialization needed
  // ctime is already set in allocproc() to current ticks
  if(p == 0) {
    // Initialize scheduler state (called from sched_init)
  }
}

// Select next RUNNABLE process using FCFS (First-Come-First-Served)
// Returns the process with the smallest ctime (earliest creation time)
// This is a non-preemptive algorithm - once selected, process runs to completion
static struct proc*
fcfs_pick_next(void)
{
  struct proc *p;
  struct proc *selected = 0;
  struct cpu *c = mycpu();
  uint64 earliest_ctime = 0xffffffffffffffffULL;  // Max uint64

  // Find the RUNNABLE process with the earliest creation time
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == RUNNABLE) {
      if(p->ctime < earliest_ctime) {
        // Found an earlier process
        // Release lock of previously selected process (if any)
        if(selected != 0 && selected != p) {
          release(&selected->lock);
        }
        selected = p;
        earliest_ctime = p->ctime;
        // Keep the lock of the selected process
      } else {
        release(&p->lock);
      }
    } else {
      release(&p->lock);
    }
  }

  // If we found a runnable process, switch to it
  if(selected != 0) {
    // selected->lock is still held from the loop above
    selected->state = RUNNING;
    c->proc = selected;
    swtch(&c->context, &selected->context);

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
    release(&selected->lock);
    return selected;
  }

  // No runnable process found
  return 0;
}

// Process exit hook for FCFS scheduler
static void
fcfs_proc_exit(struct proc *p)
{
  // No special cleanup needed for FCFS
}

// Scheduler operations table for FCFS policy
struct sched_ops fcfs_ops = {
  .name = "FCFS",
  .proc_init = fcfs_proc_init,
  .pick_next = fcfs_pick_next,
  .proc_exit = fcfs_proc_exit,
};
