// ps.c - Process status display tool for xv6

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_PSTAT 64

static char *state_names[] = {
  "UNUSED", "USED", "SLEEP", "RUNBL", "RUN  ", "ZOMBIE"
};

static char *sched_names[] = {
  "DEFAULT", "FCFS", "PRIOR", "SML", "LOT"
};

void
print_header(void)
{
  printf("PID   STATE    SCHED    PRI  TICK  SZ       RUNT  RETM  SLP  NAME\n");
  printf("----  -------  -------  ---  ----  -------  ----  ----  ---  ----\n");
}

void
print_process(struct pstat *p)
{
  printf("%-4d  %-7s  %-7s  %-3d  %-4d  %-7d  %-4d  %-4d  %-3d  %s\n",
         p->pid,
         state_names[p->state],
         sched_names[p->sched_class],
         p->priority,
         p->tickets,
         p->sz / 1024,
         p->rutime,
         p->retime,
         p->stime,
         p->name);
}

int
main(int argc, char *argv[])
{
  struct pstat ps[MAX_PSTAT];
  int count;
  int show_sched = -1;

  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
      show_sched = atoi(argv[++i]);
    } else if(strcmp(argv[i], "-h") == 0) {
      printf("Usage: ps [-s <scheduler>]\n");
      printf("  -s  Filter by scheduler (0-4)\n");
      printf("Schedulers: 0=DEFAULT, 1=FCFS, 2=PRIORITY, 3=SML, 4=LOTTERY\n");
      exit(0);
    }
  }

  count = getptable(ps, MAX_PSTAT);

  if(count < 0) {
    printf("ps: failed to get process table\n");
    exit(1);
  }

  print_header();

  for(int i = 0; i < count; i++) {
    if(show_sched >= 0 && ps[i].sched_class != show_sched) {
      continue;
    }
    print_process(&ps[i]);
  }

  printf("\nTotal: %d processes\n", count);
  exit(0);
}
