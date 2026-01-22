// SML (Static Multilevel Queue) scheduler
// Three priority queues: HIGH (class 1), MEDIUM (class 2), LOW (class 3)
// - Higher class = higher priority (lower number)
// - Each class uses Round-Robin within the class
// - Always check higher classes first before lower classes
// - Preemptive: higher class processes always selected over lower class

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sched/sched.h"
#include "kernel/defs.h"

// Round-robin index for each class
static int rr_index[4] = {0, 0, 0, 0};  // Index 1-3 used for classes 1-3

// Process initialization hook for SML scheduler
// sched_class is already set in allocproc() to DEFAULT_SML_CLASS (2)
static void
sml_proc_init(struct proc *p)
{
  // No special initialization needed
  // sched_class is already set in allocproc() to DEFAULT_SML_CLASS
  if(p == 0) {
    // Initialize scheduler state (called from sched_init)
    rr_index[1] = rr_index[2] = rr_index[3] = 0;
  }
}

// Select next RUNNABLE process using SML (Static Multilevel Queue)
// Checks queues in order: class 1 (HIGH) -> class 2 (MEDIUM) -> class 3 (LOW)
// Within each class, uses round-robin starting from rr_index[class]
static struct proc*
sml_pick_next(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  // Check each class in priority order (1 = highest, 3 = lowest)
  for(int class = 1; class <= 3; class++) {
    int start_index = rr_index[class];
    int found = 0;

    // Scan through process table starting from rr_index for this class
    for(int i = 0; i < NPROC; i++) {
      p = &proc[(start_index + i) % NPROC];

      acquire(&p->lock);
      if(p->state == RUNNABLE && p->sched_class == class) {
        // Found a runnable process in this class
        rr_index[class] = (start_index + i + 1) % NPROC;  // Next time, start after this one

        // Switch to chosen process
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        found = 1;
        release(&p->lock);
        return p;  // Return the process we just ran
      }
      release(&p->lock);
    }

    // If we found a process in this class, return it
    if(found) {
      return p;
    }
  }

  // No runnable process found
  return 0;
}

// Process exit hook for SML scheduler
static void
sml_proc_exit(struct proc *p)
{
  // No special cleanup needed for SML
}

// Scheduler operations table for SML policy
struct sched_ops sml_ops = {
  .name = "SML",
  .proc_init = sml_proc_init,
  .pick_next = sml_pick_next,
  .proc_exit = sml_proc_exit,
};
