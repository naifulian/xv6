// LOTTERY (Lottery Scheduling) scheduler
// Probabilistic scheduling: each process holds "tickets"
// The scheduler draws a random ticket and selects the corresponding process
// More tickets = higher probability of being selected
// This provides proportional share scheduling

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
#include "kernel/sched/sched.h"
#include "kernel/defs.h"

// Process initialization hook for LOTTERY scheduler
// tickets is already set in allocproc() to DEFAULT_TICKETS (1)
static void
lottery_proc_init(struct proc *p)
{
  // No special initialization needed
  // tickets is already set in allocproc() to DEFAULT_TICKETS
  if(p == 0) {
    // Initialize scheduler state (called from sched_init)
  }
}

// Simple pseudorandom number generator
// Uses a linear congruential generator
static uint64
lottery_random(uint64 *seed)
{
  // LCG parameters from Numerical Recipes
  *seed = *seed * 1664525ULL + 1013904223ULL;
  return *seed;
}

// Select next RUNNABLE process using LOTTERY scheduling
// Each process's tickets represent its share of CPU time
// Returns the process selected by lottery drawing
static struct proc*
lottery_pick_next(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  int total_tickets = 0;
  static uint64 seed = 12345;  // Seed for random number generation

  // First pass: count total tickets among all runnable processes
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == RUNNABLE) {
      total_tickets += p->tickets;
    }
    release(&p->lock);
  }

  // If no runnable process or no tickets, return 0
  if(total_tickets <= 0) {
    return 0;
  }

  // Draw a winning ticket
  int winning_ticket = (int)(lottery_random(&seed) % total_tickets);
  int current_ticket = 0;

  // Second pass: find the process that holds the winning ticket
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == RUNNABLE) {
      int tickets = p->tickets;
      if(current_ticket + tickets > winning_ticket) {
        // This process holds the winning ticket
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        release(&p->lock);
        return p;
      }
      current_ticket += tickets;
    }
    release(&p->lock);
  }

  // Should never reach here if there are runnable processes
  return 0;
}

// Process exit hook for LOTTERY scheduler
static void
lottery_proc_exit(struct proc *p)
{
  // No special cleanup needed for LOTTERY
}

// Scheduler operations table for LOTTERY policy
struct sched_ops lottery_ops = {
  .name = "LOTTERY",
  .proc_init = lottery_proc_init,
  .pick_next = lottery_pick_next,
  .proc_exit = lottery_proc_exit,
};
