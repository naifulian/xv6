// Default scheduler: Round-Robin
// Refactored from kernel/proc.c scheduler() function
// This is the original xv6-riscv scheduling behavior

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sched/sched.h"
#include "kernel/defs.h"

// Start index for round-robin scan
// Ensures fairness by continuing where we left off
static int rr_index = 0;

// Process initialization hook for DEFAULT scheduler
// Called from sched_init() and allocproc()
static void
default_proc_init(struct proc *p)
{
  // No special initialization needed for Round-Robin
  // The rr_index will be reset in pick_next()
  if(p == 0) {
    // Initialize scheduler state (called from sched_init)
    rr_index = 0;
  }
}

// Select next RUNNABLE process using round-robin
// This is the core scheduling logic for DEFAULT policy
// Returns the selected process with its lock held
static struct proc*
default_pick_next(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  int start_index = rr_index;
  int found = 0;

  // Scan through process table starting from rr_index
  // This implements round-robin by continuing where we left off
  for(int i = 0; i < NPROC; i++) {
    p = &proc[(start_index + i) % NPROC];

    acquire(&p->lock);
    if(p->state == RUNNABLE) {
      // Found a runnable process
      rr_index = (start_index + i + 1) % NPROC;  // Next time, start after this one

      // Switch to chosen process
      p->state = RUNNING;
      c->proc = p;
      swtch(&c->context, &p->context);

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      found = 1;
    }
    release(&p->lock);

    // If we found and ran a process, break out of the loop
    // This gives each process a time slice before moving to the next
    if(found) {
      return p;  // Return the process we just ran (for consistency)
    }
  }

  // No runnable process found
  return 0;
}

// Process exit hook for DEFAULT scheduler
static void
default_proc_exit(struct proc *p)
{
  // No special cleanup needed for Round-Robin
}

// Scheduler operations table for DEFAULT (Round-Robin) policy
struct sched_ops default_ops = {
  .name = "DEFAULT",
  .proc_init = default_proc_init,
  .pick_next = default_pick_next,
  .proc_exit = default_proc_exit,
};
