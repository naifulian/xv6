// Core scheduler module for xv6-riscv
// Supports runtime dynamic switching between scheduling algorithms
// Similar to Linux scheduler architecture

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sched/sched.h"
#include "kernel/defs.h"

// Lock for scheduler policy switching
static struct spinlock sched_lock;

// Current active scheduling policy
enum sched_policy current_policy = SCHED_DEFAULT;
struct sched_ops *current_ops = 0;  // Will be set in sched_init()

// Forward declarations for each scheduler's operations
extern struct sched_ops default_ops;   // sched_default.c
extern struct sched_ops fcfs_ops;      // sched_fcfs.c
extern struct sched_ops priority_ops;  // sched_priority.c
extern struct sched_ops sml_ops;       // sched_sml.c
extern struct sched_ops lottery_ops;   // sched_lottery.c
extern struct sched_ops sjf_ops;       // sched_sjf.c
extern struct sched_ops srtf_ops;      // sched_srtf.c

// Scheduler operations table (maps policy to implementation)
static struct sched_ops* sched_ops_table[SCHED_SRTF + 1] = {
    [SCHED_DEFAULT]  = &default_ops,
    [SCHED_FCFS]     = &fcfs_ops,
    [SCHED_PRIORITY] = &priority_ops,
    [SCHED_SML]      = &sml_ops,
    [SCHED_LOTTERY]  = &lottery_ops,
    [SCHED_SJF]      = &sjf_ops,
    [SCHED_SRTF]     = &srtf_ops,
};

// Initialize the scheduler subsystem
// Called from main() during kernel initialization
void
sched_init(void)
{
  // Initialize the scheduler lock
  initlock(&sched_lock, "sched");

  // Set initial policy to DEFAULT (Round-Robin)
  current_policy = SCHED_DEFAULT;
  current_ops = &default_ops;

  // Call proc_init hook for default scheduler
  if(current_ops->proc_init) {
    current_ops->proc_init(0);
  }
}

// Select the next process to run
// Delegates to the current scheduler's pick_next() function
// Returns the selected process with its lock held
struct proc*
sched_pick_next(void)
{
  if(current_ops && current_ops->pick_next) {
    return current_ops->pick_next();
  }
  // Should never happen if sched_init() was called
  panic("sched_pick_next: no scheduler operations");
}

// Switch to a different scheduling policy at runtime
// Similar to Linux's sched_setScheduler() system call
int
sched_set_policy(int policy)
{
  // Validate policy number
  if(policy < 0 || policy > SCHED_SRTF) {
    return -1;
  }

  // Check if the scheduler is implemented
  if(sched_ops_table[policy] == 0) {
    return -1;  // Policy not yet implemented
  }

  // Switch to new policy with lock protection
  acquire(&sched_lock);
  current_policy = policy;
  current_ops = sched_ops_table[policy];
  release(&sched_lock);

  return 0;
}

// Get the name of a scheduling policy
const char*
sched_policy_name(int policy)
{
  switch(policy) {
    case SCHED_DEFAULT:
      return "DEFAULT";
    case SCHED_FCFS:
      return "FCFS";
    case SCHED_PRIORITY:
      return "PRIORITY";
    case SCHED_SML:
      return "SML";
    case SCHED_LOTTERY:
      return "LOTTERY";
    case SCHED_SJF:
      return "SJF";
    case SCHED_SRTF:
      return "SRTF";
    default:
      return "UNKNOWN";
  }
}
