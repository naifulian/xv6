#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "sched/sched.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Set the scheduling policy
// Argument: policy number (0=DEFAULT, 1=FCFS, 2=PRIORITY, 3=SML, 4=LOTTERY)
uint64
sys_setscheduler(void)
{
  int policy;

  argint(0, &policy);

  if(sched_set_policy(policy) < 0)
    return -1;

  return 0;
}

// Get the current scheduling policy
// Returns: policy number
uint64
sys_getscheduler(void)
{
  return current_policy;
}

// mmap system call
// Simplified version: supports MAP_ANONYMOUS | MAP_PRIVATE only
// Parameters: addr (ignored), length, prot, flags, fd (ignored), offset (ignored)
uint64
sys_mmap(void)
{
  uint64 addr, length, prot, flags, fd, offset;
  struct proc *p = myproc();

  argaddr(0, &addr);
  argaddr(1, &length);
  argaddr(2, &prot);
  argaddr(3, &flags);
  argaddr(4, &fd);
  argaddr(5, &offset);

  // Only support MAP_ANONYMOUS | MAP_PRIVATE for now
  if((flags & 0x02) == 0)  // MAP_PRIVATE = 0x02
    return -1;
  if((flags & 0x20) == 0)  // MAP_ANONYMOUS = 0x20
    return -1;

  // Check length
  if(length == 0)
    return 0;

  // Align to page boundary
  length = PGROUNDUP(length);

  // Find a valid virtual address (after current sz)
  uint64 new_addr = p->sz;

  // Check for overflow
  if(new_addr + length > MAXVA)
    return -1;

  // Lazy allocation: just update process size, don't allocate physical pages yet
  // Pages will be allocated on demand (page fault) via vmfault()
  p->sz += length;

  return new_addr;
}

// munmap system call
// Parameters: addr, length
uint64
sys_munmap(void)
{
  uint64 addr, length;
  struct proc *p = myproc();

  argaddr(0, &addr);
  argaddr(1, &length);

  if(length == 0)
    return 0;

  // Align to page boundary
  addr = PGROUNDDOWN(addr);
  length = PGROUNDUP(length);

  // Check if address is valid
  if(addr >= p->sz)
    return -1;

  // Check if it's at the end of address space
  if(addr + length != p->sz)
    return -1;  // Only support unmapping from the end

  // Unmap and shrink
  p->sz = addr;
  uvmdealloc(p->pagetable, addr + length, addr);
  return 0;
}
