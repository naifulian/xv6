// System snapshot data structure for monitoring
// Collects system-wide statistics for sysmon tool

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sched/sched.h"
#include "kernel/defs.h"

struct sys_snapshot {
  uint64 timestamp;
  
  int cpu_usage;
  uint64 context_switches;
  uint64 total_ticks;
  
  int total_pages;
  int free_pages;
  
  int nr_running;
  int nr_sleeping;
  int nr_zombie;
  int nr_total;
  
  int sched_policy;
  int runqueue_len;
};

static struct spinlock snapshot_lock;

struct cpu_stats {
  uint64 total_ticks;
  uint64 idle_ticks;
  uint64 context_switches;
  uint64 interrupts;
  uint64 proc_created;
  uint64 proc_exited;
  uint64 syscalls;
};

extern void get_cpu_stats(struct cpu_stats*);

struct memstat {
  uint total_pages;
  uint free_pages;
  uint buddy_merges;
  uint buddy_splits;
  uint cow_faults;
  uint lazy_allocs;
  uint cow_copy_pages;
};

extern void fill_memstat(void*);

void
sys_snapshot_init(void)
{
  initlock(&snapshot_lock, "snapshot");
}

void
take_snapshot(struct sys_snapshot *snap)
{
  if(snap == 0) return;
  
  acquire(&snapshot_lock);
  
  snap->timestamp = ticks;
  snap->sched_policy = current_policy;
  
  struct cpu_stats cstats;
  get_cpu_stats(&cstats);
  snap->total_ticks = cstats.total_ticks;
  snap->context_switches = cstats.context_switches;
  snap->cpu_usage = cstats.total_ticks > 0 ? 
    (int)((cstats.total_ticks - cstats.idle_ticks) * 100 / cstats.total_ticks) : 0;
  
  struct memstat mstat;
  fill_memstat(&mstat);
  snap->total_pages = mstat.total_pages;
  snap->free_pages = mstat.free_pages;
  
  snap->nr_running = 0;
  snap->nr_sleeping = 0;
  snap->nr_zombie = 0;
  snap->nr_total = 0;
  snap->runqueue_len = 0;
  
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state != UNUSED) {
      snap->nr_total++;
      switch(p->state) {
        case RUNNABLE:
          snap->nr_running++;
          snap->runqueue_len++;
          break;
        case SLEEPING:
          snap->nr_sleeping++;
          break;
        case ZOMBIE:
          snap->nr_zombie++;
          break;
        case RUNNING:
          snap->nr_running++;
          break;
        default:
          break;
      }
    }
    release(&p->lock);
  }
  
  release(&snapshot_lock);
}

uint64
sys_getsnapshot(void)
{
  uint64 addr;
  struct sys_snapshot snap;
  struct proc *p = myproc();
  
  argaddr(0, &addr);
  
  if(addr == 0)
    return -1;
  
  take_snapshot(&snap);
  
  if(copyout(p->pagetable, addr, (char*)&snap, sizeof(snap)) < 0)
    return -1;
  
  return 0;
}
