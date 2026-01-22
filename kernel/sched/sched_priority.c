// PRIORITY scheduler
// Selects the RUNNABLE process with the highest priority (lowest priority number)
// Higher priority = lower number (1 is highest, 20 is lowest)
// Preemptive: if a higher priority process becomes runnable, it will be selected next

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sched/sched.h"
#include "kernel/defs.h"

// Process initialization hook for PRIORITY scheduler
// Priority is already set in allocproc() to DEFAULT_PRIORITY (10)
static void
priority_proc_init(struct proc *p)
{
  // No special initialization needed
  // priority is already set in allocproc() to DEFAULT_PRIORITY
  if(p == 0) {
    // Initialize scheduler state (called from sched_init)
  }
}

// Select next RUNNABLE process using PRIORITY scheduling
// Returns the process with the highest priority (lowest priority number)
// If multiple processes have the same priority, uses FCFS (earliest ctime)
static struct proc*
priority_pick_next(void)
{
  struct proc *p;
  struct proc *selected = 0;
  struct cpu *c = mycpu();
  int highest_priority = MAX_PRIORITY + 1;  // Start higher than any valid priority
  uint64 earliest_ctime = 0xffffffffffffffffULL;  // For tie-breaking

  // Find the RUNNABLE process with the highest priority
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == RUNNABLE) {
      // Higher priority = lower number
      if(p->priority < highest_priority ||
         (p->priority == highest_priority && p->ctime < earliest_ctime)) {
        // Found a higher priority process, or same priority but earlier
        // Release lock of previously selected process (if any)
        if(selected != 0 && selected != p) {
          release(&selected->lock);
        }
        selected = p;
        highest_priority = p->priority;
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

// Process exit hook for PRIORITY scheduler
static void
priority_proc_exit(struct proc *p)
{
  // No special cleanup needed for PRIORITY
}

// Scheduler operations table for PRIORITY policy
struct sched_ops priority_ops = {
  .name = "PRIORITY",
  .proc_init = priority_proc_init,
  .pick_next = priority_pick_next,
  .proc_exit = priority_proc_exit,
};
