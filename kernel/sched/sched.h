// Scheduler interface for xv6-riscv
// Supports runtime dynamic switching between scheduling algorithms

#ifndef SCHED_H
#define SCHED_H

#include "kernel/types.h"  // For uint64, int types
#include "kernel/param.h"  // For NPROC

// Forward declaration to avoid circular dependency
struct proc;

// Scheduling policy enumeration
enum sched_policy {
    SCHED_DEFAULT,    // Round-Robin (default)
    SCHED_FCFS,       // First-Come-First-Served
    SCHED_PRIORITY,   // Priority Scheduling
    SCHED_SML,        // Static Multilevel Queue
    SCHED_LOTTERY     // Lottery Scheduling
};

// Scheduler operations interface (Strategy Pattern)
struct sched_ops {
    const char *name;
    // Process initialization hook (optional, can be NULL)
    void (*proc_init)(struct proc *p);
    // Select next process to run (required)
    struct proc* (*pick_next)(void);
    // Process exit cleanup hook (optional, can be NULL)
    void (*proc_exit)(struct proc *p);
};

// Global variables (defined in sched.c)
extern enum sched_policy current_policy;  // Current scheduling policy
extern struct sched_ops *current_ops;     // Current scheduler operations

// Core scheduler functions
void sched_init(void);
struct proc* sched_pick_next(void);
void sched_update_stats(void);
int sched_set_policy(int policy);
const char* sched_policy_name(int policy);

// Default values for scheduler fields
#define DEFAULT_PRIORITY    10    // Default priority for PRIORITY scheduler
#define DEFAULT_SML_CLASS    2    // Default class for SML scheduler (medium)
#define DEFAULT_TICKETS      1    // Default tickets for LOTTERY scheduler
#define MIN_PRIORITY         1    // Minimum priority value (highest priority)
#define MAX_PRIORITY         20   // Maximum priority value (lowest priority)
#define MIN_SML_CLASS        1    // Minimum SML class (highest priority)
#define MAX_SML_CLASS        3    // Maximum SML class (lowest priority)
#define MIN_TICKETS          1    // Minimum tickets
#define MAX_TICKETS          100  // Maximum tickets

#endif // SCHED_H
