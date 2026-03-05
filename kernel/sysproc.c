#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "sched/sched.h"

struct memstat {
  uint total_pages;
  uint free_pages;
  uint buddy_merges;
  uint buddy_splits;
  uint cow_faults;
  uint lazy_allocs;
  uint cow_copy_pages;
};

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

  addr = PGROUNDDOWN(addr);
  length = PGROUNDUP(length);

  if(addr >= p->sz || addr + length > p->sz)
    return -1;

  uvmunmap(p->pagetable, addr, length / PGSIZE, 1);

  if(addr + length == p->sz)
    p->sz = addr;

  return 0;
}

// Process info structure for user space
struct pstat {
  int pid;
  int state;
  int priority;
  int tickets;
  int sched_class;
  uint sz;
  int rutime;
  int retime;
  int stime;
  char name[16];
};

// Get process table snapshot
// Parameters: addr (buffer pointer), max_count (max entries)
// Returns: number of processes copied
uint64
sys_getptable(void)
{
  uint64 addr;
  int max_count;
  struct pstat *buf;
  struct proc *p;
  int count = 0;

  argaddr(0, &addr);
  argint(1, &max_count);

  if(addr == 0 || max_count <= 0)
    return -1;

  buf = (struct pstat *)kalloc();
  if(buf == 0)
    return -1;

  memset(buf, 0, PGSIZE);

  for(p = proc; p < &proc[NPROC] && count < max_count; p++) {
    acquire(&p->lock);
    if(p->state != UNUSED) {
      buf[count].pid = p->pid;
      buf[count].state = p->state;
      buf[count].priority = p->priority;
      buf[count].tickets = p->tickets;
      buf[count].sched_class = p->sched_class;
      buf[count].sz = p->sz;
      buf[count].rutime = p->rutime;
      buf[count].retime = p->retime;
      buf[count].stime = p->stime;
      safestrcpy(buf[count].name, p->name, sizeof(buf[count].name));
      count++;
    }
    release(&p->lock);
  }

  // Copy to user space
  struct proc *cur = myproc();
  if(copyout(cur->pagetable, addr, (char *)buf, count * sizeof(struct pstat)) < 0) {
    kfree((void *)buf);
    return -1;
  }

  kfree((void *)buf);
  return count;
}

// Get memory statistics
// Parameters: addr (memstat structure pointer)
// Returns: 0 on success, -1 on failure
uint64
sys_getmemstat(void)
{
  uint64 addr;
  struct memstat buf;
  struct proc *p = myproc();

  argaddr(0, &addr);

  if(addr == 0)
    return -1;

  fill_memstat(&buf);

  if(copyout(p->pagetable, addr, (char*)&buf, sizeof(buf)) < 0)
    return -1;

  return 0;
}

// Set process priority
// Parameters: pid, priority (1-20)
// Returns: 0 on success, -1 on failure
uint64
sys_setpriority(void)
{
  int pid, priority;
  struct proc *p;

  argint(0, &pid);
  argint(1, &priority);

  if(priority < 1 || priority > 20)
    return -1;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->pid == pid) {
      p->priority = priority;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }

  return -1;
}
