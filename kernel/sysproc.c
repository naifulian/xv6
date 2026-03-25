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

static int
alloc_vma_slot(struct proc *p)
{
  for(int i = 0; i < NVMA; i++) {
    if(p->vmas[i].valid == 0)
      return i;
  }
  return -1;
}

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
sys_telemetrywrite(void)
{
  uint64 src;
  int n;
  char buf[512];
  int copied = 0;
  struct proc *p = myproc();

  argaddr(0, &src);
  argint(1, &n);
  if(n < 0)
    return -1;

  while(copied < n){
    int chunk = n - copied;
    if(chunk > sizeof(buf))
      chunk = sizeof(buf);
    if(copyin(p->pagetable, buf, src + copied, chunk) < 0)
      return copied > 0 ? copied : -1;
    if(telemetry_console_write(buf, chunk) < 0)
      return copied > 0 ? copied : -1;
    copied += chunk;
  }

  return copied;
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
  struct proc *p = myproc();
  uint64 addr = p->heap_end;
  uint64 new_heap;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0)
      return -1;
  } else {
    new_heap = addr + n;
    if(new_heap < addr)
      return -1;
    if(new_heap > proc_mmap_limit(p))
      return -1;
    p->heap_end = new_heap;
    if(new_heap > p->sz)
      p->sz = new_heap;
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

uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_setscheduler(void)
{
  int policy;

  argint(0, &policy);
  if(sched_set_policy(policy) < 0)
    return -1;
  return 0;
}

uint64
sys_getscheduler(void)
{
  return current_policy;
}

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

  if((flags & MAP_PRIVATE) == 0 || (flags & MAP_ANONYMOUS) == 0)
    return -1;
  if(fd != (uint64)-1 || offset != 0)
    return -1;
  if((prot & (PROT_READ | PROT_WRITE)) == 0)
    return -1;
  if(length == 0)
    return 0;

  length = PGROUNDUP(length);
  int slot = alloc_vma_slot(p);
  if(slot < 0)
    return -1;

  uint64 new_addr = PGROUNDUP(p->sz);
  if(new_addr + length > TRAPFRAME)
    return -1;

  p->vmas[slot].addr = new_addr;
  p->vmas[slot].length = length;
  p->vmas[slot].prot = (int)prot;
  p->vmas[slot].flags = (int)flags;
  p->vmas[slot].valid = 1;
  p->sz = new_addr + length;

  return new_addr;
}

uint64
sys_munmap(void)
{
  uint64 addr, length;
  struct proc *p = myproc();
  struct vma *vma = 0;
  int split_slot = -1;

  argaddr(0, &addr);
  argaddr(1, &length);

  if(length == 0)
    return 0;

  addr = PGROUNDDOWN(addr);
  length = PGROUNDUP(length);

  for(int i = 0; i < NVMA; i++) {
    if(p->vmas[i].valid == 0)
      continue;
    uint64 start = p->vmas[i].addr;
    uint64 end = start + p->vmas[i].length;
    if(addr >= start && addr + length <= end) {
      vma = &p->vmas[i];
      if(addr > start && addr + length < end) {
        split_slot = alloc_vma_slot(p);
        if(split_slot < 0)
          return -1;
      }
      break;
    }
  }

  if(vma == 0)
    return -1;

  uvmunmap(p->pagetable, addr, length / PGSIZE, 1);

  uint64 start = vma->addr;
  uint64 end = start + vma->length;
  uint64 unmap_end = addr + length;

  if(addr == start && unmap_end == end) {
    vma->valid = 0;
  } else if(addr == start) {
    vma->addr = unmap_end;
    vma->length = end - unmap_end;
  } else if(unmap_end == end) {
    vma->length = addr - start;
  } else {
    p->vmas[split_slot].addr = unmap_end;
    p->vmas[split_slot].length = end - unmap_end;
    p->vmas[split_slot].prot = vma->prot;
    p->vmas[split_slot].flags = vma->flags;
    p->vmas[split_slot].valid = 1;
    vma->length = addr - start;
  }

  p->sz = proc_vm_top(p);
  return 0;
}

struct pstat {
  int pid;
  int state;
  int priority;
  int tickets;
  int sched_class;
  uint sz;
  uint heap_end;
  int vma_count;
  int mmap_regions;
  uint mmap_bytes;
  int rutime;
  int retime;
  int stime;
  char name[16];
};

struct vma_info {
  int pid;
  int slot;
  uint64 start;
  uint64 end;
  uint64 length;
  int prot;
  int flags;
};

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
      int vma_count = 0;
      uint mmap_bytes = 0;

      for(int i = 0; i < NVMA; i++) {
        if(p->vmas[i].valid) {
          vma_count++;
          mmap_bytes += p->vmas[i].length;
        }
      }

      buf[count].pid = p->pid;
      buf[count].state = p->state;
      buf[count].priority = p->priority;
      buf[count].tickets = p->tickets;
      buf[count].sched_class = p->sched_class;
      buf[count].sz = p->sz;
      buf[count].heap_end = p->heap_end;
      buf[count].vma_count = vma_count;
      buf[count].mmap_regions = vma_count;
      buf[count].mmap_bytes = mmap_bytes;
      buf[count].rutime = p->rutime;
      buf[count].retime = p->retime;
      buf[count].stime = p->stime;
      safestrcpy(buf[count].name, p->name, sizeof(buf[count].name));
      count++;
    }
    release(&p->lock);
  }

  struct proc *cur = myproc();
  if(copyout(cur->pagetable, addr, (char *)buf, count * sizeof(struct pstat)) < 0) {
    kfree((void *)buf);
    return -1;
  }

  kfree((void *)buf);
  return count;
}

uint64
sys_getprocvmas(void)
{
  int pid;
  uint64 addr;
  int max_count;
  struct vma_info *buf;
  struct proc *target = 0;
  int count = 0;

  argint(0, &pid);
  argaddr(1, &addr);
  argint(2, &max_count);

  if(pid < 1 || addr == 0 || max_count <= 0)
    return -1;

  for(struct proc *p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state != UNUSED && p->pid == pid) {
      target = p;
      break;
    }
    release(&p->lock);
  }

  if(target == 0)
    return -1;

  buf = (struct vma_info *)kalloc();
  if(buf == 0) {
    release(&target->lock);
    return -1;
  }

  memset(buf, 0, PGSIZE);

  for(int i = 0; i < NVMA && count < max_count; i++) {
    if(target->vmas[i].valid == 0)
      continue;
    buf[count].pid = target->pid;
    buf[count].slot = i;
    buf[count].start = target->vmas[i].addr;
    buf[count].end = target->vmas[i].addr + target->vmas[i].length;
    buf[count].length = target->vmas[i].length;
    buf[count].prot = target->vmas[i].prot;
    buf[count].flags = target->vmas[i].flags;
    count++;
  }

  release(&target->lock);

  struct proc *cur = myproc();
  if(copyout(cur->pagetable, addr, (char *)buf, count * sizeof(struct vma_info)) < 0) {
    kfree((void *)buf);
    return -1;
  }

  kfree((void *)buf);
  return count;
}

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

uint64
sys_getstats(void)
{
  extern int sys_getstats_kernel(void);
  return sys_getstats_kernel();
}
