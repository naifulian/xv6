// sysmon.c - emit structured monitoring snapshots for the host-side web UI

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_PSTAT 64

static char *state_names[] = {
  "UNUSED", "USED", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE"
};

static char *sched_names[] = {
  "DEFAULT", "FCFS", "PRIORITY", "SML", "LOTTERY",
  "SJF", "SRTF", "MLFQ", "CFS"
};

// xv6 user stack is only one page, so large monitoring buffers must not live on stack.
static struct sys_snapshot snap;
static struct memstat ms;
static struct pstat ps[MAX_PSTAT];

static const char *
state_name(int state)
{
  if(state >= 0 && state <= 5)
    return state_names[state];
  return "UNKNOWN";
}

static const char *
sched_name(int sched_class)
{
  if(sched_class >= 0 && sched_class <= 8)
    return sched_names[sched_class];
  return "UNKNOWN";
}

int
main(int argc, char *argv[])
{
  int interval = 10;
  int samples = 12;
  if(argc > 1)
    interval = atoi(argv[1]);
  if(argc > 2)
    samples = atoi(argv[2]);

  if(interval < 1)
    interval = 1;
  if(samples < 1)
    samples = 1;

  telemetry_printf("SYSMON_BEGIN interval=%d samples=%d\n", interval, samples);

  for(int seq = 0; seq < samples; seq++) {
    if(getsnapshot(&snap) < 0) {
      telemetry_printf("SYSMON_ERROR stage=getsnapshot seq=%d\n", seq);
      exit(1);
    }
    if(getmemstat(&ms) < 0) {
      telemetry_printf("SYSMON_ERROR stage=getmemstat seq=%d\n", seq);
      exit(1);
    }

    int count = getptable(ps, MAX_PSTAT);
    if(count < 0) {
      telemetry_printf("SYSMON_ERROR stage=getptable seq=%d\n", seq);
      exit(1);
    }

    telemetry_printf("SNAP seq=%d ts=%d sched=%s policy=%d cpu=%d ctx=%d ticks=%d total_pages=%d free_pages=%d nr_total=%d nr_running=%d nr_sleeping=%d nr_zombie=%d runqueue=%d buddy_merges=%d buddy_splits=%d cow_faults=%d lazy_allocs=%d cow_copy_pages=%d\n",
                     seq, (int)snap.timestamp, snap.sched_name, snap.sched_policy,
                     snap.cpu_usage, (int)snap.context_switches, (int)snap.total_ticks,
                     snap.total_pages, snap.free_pages, snap.nr_total, snap.nr_running,
                     snap.nr_sleeping, snap.nr_zombie, snap.runqueue_len,
                     (int)ms.buddy_merges, (int)ms.buddy_splits, (int)ms.cow_faults,
                     (int)ms.lazy_allocs, (int)ms.cow_copy_pages);

    for(int i = 0; i < count; i++) {
      struct pstat *p = &ps[i];
      telemetry_printf("PROC seq=%d pid=%d state=%s priority=%d tickets=%d sched=%s sched_class=%d sz=%d heap_end=%d vma_count=%d mmap_regions=%d mmap_bytes=%d rutime=%d retime=%d stime=%d name=%s\n",
                       seq, p->pid, state_name(p->state), p->priority, p->tickets,
                       sched_name(p->sched_class), p->sched_class, (int)p->sz, (int)p->heap_end,
                       p->vma_count, p->mmap_regions, (int)p->mmap_bytes, p->rutime, p->retime,
                       p->stime, p->name);
    }

    if(seq + 1 < samples)
      sleep(interval);
  }

  telemetry_printf("SYSMON_END samples=%d\n", samples);
  exit(0);
}
