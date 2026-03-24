// MLFQ (Multi-Level Feedback Queue) scheduler
// Multi-level queue with dynamic priority adjustment

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
    for(int i = 0; i < NQUEUE; i++)
      rr_index[i] = 0;
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

static void
mlfq_proc_tick(struct proc *p)
{
  if(p == 0)
    return;

  int idx = p - proc;
  int level = queue_level[idx];
  if(level < 0 || level >= NQUEUE)
    return;

  time_in_queue[idx]++;
  if(time_in_queue[idx] >= timeslice[level]) {
    if(queue_level[idx] < NQUEUE - 1)
      queue_level[idx]++;
    time_in_queue[idx] = 0;
  }
}

static void
mlfq_proc_wakeup(struct proc *p)
{
  if(p == 0)
    return;

  int idx = p - proc;
  if(queue_level[idx] > 0)
    queue_level[idx]--;
  time_in_queue[idx] = 0;
}

static void
mlfq_proc_exit(struct proc *p)
{
  if(p == 0)
    return;

  int idx = p - proc;
  queue_level[idx] = DEFAULT_QUEUE_LEVEL;
  time_in_queue[idx] = 0;
}

struct sched_ops mlfq_ops = {
  .name = "MLFQ",
  .proc_init = mlfq_proc_init,
  .pick_next = mlfq_pick_next,
  .proc_exit = mlfq_proc_exit,
  .proc_tick = mlfq_proc_tick,
  .proc_wakeup = mlfq_proc_wakeup,
};
