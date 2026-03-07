// MLFQ (Multi-Level Feedback Queue) scheduler
// Multi-level queue with dynamic priority adjustment
// New processes start at highest priority
// Processes demoted after using time slice
// I/O-bound processes promoted (interactive tasks)

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sched/sched.h"
#include "kernel/defs.h"

#define NQUEUE              3
#define DEFAULT_QUEUE_LEVEL 0

static int timeslice[NQUEUE] = {10, 20, 40};
static int rr_index[NQUEUE];
static int queue_level[NPROC];
static int time_in_queue[NPROC];

static void
mlfq_proc_init(struct proc *p)
{
  if(p == 0) {
    for(int i = 0; i < NQUEUE; i++) {
      rr_index[i] = 0;
    }
    for(int i = 0; i < NPROC; i++) {
      queue_level[i] = DEFAULT_QUEUE_LEVEL;
      time_in_queue[i] = 0;
    }
    return;
  }
  
  int idx = p - proc;
  queue_level[idx] = DEFAULT_QUEUE_LEVEL;
  time_in_queue[idx] = 0;
}

static struct proc*
mlfq_pick_next(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  for(int q = 0; q < NQUEUE; q++) {
    for(int i = 0; i < NPROC; i++) {
      int idx = (rr_index[q] + i) % NPROC;
      p = &proc[idx];
      
      acquire(&p->lock);
      if(p->state == RUNNABLE && queue_level[idx] == q) {
        rr_index[q] = (idx + 1) % NPROC;
        
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);
        
        c->proc = 0;
        release(&p->lock);
        return p;
      }
      release(&p->lock);
    }
  }
  
  return 0;
}

void
mlfq_timeslice_end(struct proc *p)
{
  if(p == 0) return;
  
  int idx = p - proc;
  if(queue_level[idx] < NQUEUE - 1) {
    queue_level[idx]++;
  }
  time_in_queue[idx] = 0;
}

void
mlfq_boost_priority(struct proc *p)
{
  if(p == 0) return;
  
  int idx = p - proc;
  if(queue_level[idx] > 0) {
    queue_level[idx]--;
  }
}

int
mlfq_get_timeslice(struct proc *p)
{
  if(p == 0) return timeslice[0];
  
  int idx = p - proc;
  return timeslice[queue_level[idx]];
}

static void
mlfq_proc_exit(struct proc *p)
{
  if(p == 0) return;
}

struct sched_ops mlfq_ops = {
  .name = "MLFQ",
  .proc_init = mlfq_proc_init,
  .pick_next = mlfq_pick_next,
  .proc_exit = mlfq_proc_exit,
};
