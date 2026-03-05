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

  struct proc *current = myproc();
  if(current) {
    acquire(&current->lock);
    if(current->state == RUNNING)
      current->rutime++;
    release(&current->lock);
  }

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p == current)
      continue;
    acquire(&p->lock);
    switch(p->state) {
    case SLEEPING:
      p->stime++;
      break;
    case RUNNABLE:
      p->retime++;
      break;
    default:
      break;
    }
    release(&p->lock);
  }
}
