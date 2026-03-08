// Statistics collection for scheduler
// Updates process statistics on each timer interrupt
// Tracks CPU usage, context switches, interrupts, etc.

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/defs.h"

struct cpu_stats {
  uint64 total_ticks;
  uint64 idle_ticks;
  uint64 context_switches;
  uint64 interrupts;
  uint64 proc_created;
  uint64 proc_exited;
  uint64 syscalls;
};

static struct cpu_stats cpu_stats;
static struct spinlock stats_lock;

void
get_cpu_stats(struct cpu_stats *buf)
{
  if(buf == 0) return;
  
  acquire(&stats_lock);
  buf->total_ticks = cpu_stats.total_ticks;
  buf->idle_ticks = cpu_stats.idle_ticks;
  buf->context_switches = cpu_stats.context_switches;
  buf->interrupts = cpu_stats.interrupts;
  buf->proc_created = cpu_stats.proc_created;
  buf->proc_exited = cpu_stats.proc_exited;
  buf->syscalls = cpu_stats.syscalls;
  release(&stats_lock);
}

void
sched_stats_init(void)
{
  initlock(&stats_lock, "stats");
  cpu_stats.total_ticks = 0;
  cpu_stats.idle_ticks = 0;
  cpu_stats.context_switches = 0;
  cpu_stats.interrupts = 0;
  cpu_stats.proc_created = 0;
  cpu_stats.proc_exited = 0;
  cpu_stats.syscalls = 0;
}

void
sched_tick(void)
{
  acquire(&stats_lock);
  cpu_stats.total_ticks++;
  release(&stats_lock);
}

void
sched_idle_tick(void)
{
  acquire(&stats_lock);
  cpu_stats.idle_ticks++;
  release(&stats_lock);
}

void
sched_context_switch(void)
{
  acquire(&stats_lock);
  cpu_stats.context_switches++;
  release(&stats_lock);
}

void
sched_interrupt(void)
{
  acquire(&stats_lock);
  cpu_stats.interrupts++;
  release(&stats_lock);
}

void
sched_proc_created(void)
{
  acquire(&stats_lock);
  cpu_stats.proc_created++;
  release(&stats_lock);
}

void
sched_proc_exited(void)
{
  acquire(&stats_lock);
  cpu_stats.proc_exited++;
  release(&stats_lock);
}

void
sched_syscall(void)
{
  acquire(&stats_lock);
  cpu_stats.syscalls++;
  release(&stats_lock);
}

int
sys_getstats_kernel(void)
{
  acquire(&stats_lock);
  
  printf("\n=== CPU Statistics ===\n");
  printf("Total ticks: %d\n", (int)cpu_stats.total_ticks);
  printf("Idle ticks: %d\n", (int)cpu_stats.idle_ticks);
  printf("CPU usage: %d%%\n", 
         cpu_stats.total_ticks > 0 ? 
         (int)((cpu_stats.total_ticks - cpu_stats.idle_ticks) * 100 / cpu_stats.total_ticks) : 0);
  printf("Context switches: %d\n", (int)cpu_stats.context_switches);
  printf("Interrupts: %d\n", (int)cpu_stats.interrupts);
  printf("Processes created: %d\n", (int)cpu_stats.proc_created);
  printf("Processes exited: %d\n", (int)cpu_stats.proc_exited);
  printf("System calls: %d\n", (int)cpu_stats.syscalls);
  
  release(&stats_lock);
  return 0;
}

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
  } else {
    sched_idle_tick();
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
  
  sched_tick();
}
