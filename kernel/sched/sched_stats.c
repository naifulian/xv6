// Statistics collection for scheduler
// Updates process statistics on each timer interrupt

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/defs.h"

// Update statistics for all processes
// Called on each timer interrupt from trap.c
void
sched_update_stats(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);  // RISC-V uses per-process locks
    switch(p->state) {
    case SLEEPING:
      p->stime++;  // Increment sleeping time
      break;
    case RUNNABLE:
      p->retime++;  // Increment ready/waiting time
      break;
    case RUNNING:
      p->rutime++;  // Increment running time
      break;
    default:
      // UNUSED, USED, ZOMBIE - no stats update
      break;
    }
    release(&p->lock);
  }
}
