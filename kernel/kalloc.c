// Buddy System Memory Allocator
// Allocates whole 4096-byte pages with buddy system algorithm

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// Buddy system: maintain multiple free lists for different block sizes
// MAX_ORDER = 10 means max block size is 2^10 * PGSIZE = 4MB
#define MAX_ORDER 10

struct buddy_free_list {
  struct run *head;
};

struct {
  struct spinlock lock;
  struct buddy_free_list free_lists[MAX_ORDER];
} kmem;

// Reference counting for COW pages
#define MAX_PAGES 40000
static int ref_count[MAX_PAGES];

// Memory statistics counters
struct memstats {
  uint total_pages;
  uint free_pages;
  uint buddy_merges;
  uint buddy_splits;
  uint cow_faults;
  uint lazy_allocs;
  uint cow_copy_pages;
};

static struct memstats memstats;

// Get page index from physical address
static int
pa2index(uint64 pa)
{
  if (pa >= KERNBASE && pa < PHYSTOP)
    return (pa - KERNBASE) / PGSIZE;
  return -1;
}

// Increment reference count for a physical page
void
ref_inc(uint64 pa)
{
  int idx = pa2index(pa);
  if (idx >= 0 && idx < MAX_PAGES) {
    acquire(&kmem.lock);
    ref_count[idx]++;
    release(&kmem.lock);
  }
}

// Decrement reference count
// Returns 1 if page should be freed (ref count reaches 0)
int
ref_dec(uint64 pa)
{
  int idx = pa2index(pa);
  if (idx >= 0 && idx < MAX_PAGES) {
    acquire(&kmem.lock);
    if (ref_count[idx] > 0)
      ref_count[idx]--;
    int should_free = (ref_count[idx] == 0);
    release(&kmem.lock);
    return should_free;
  }
  return 1;
}

// Get reference count for a physical page
int
ref_get(uint64 pa)
{
  int idx = pa2index(pa);
  if (idx >= 0 && idx < MAX_PAGES) {
    acquire(&kmem.lock);
    int count = ref_count[idx];
    release(&kmem.lock);
    return count;
  }
  return 0;
}

// Get the buddy of a physical address for given order
// Buddy blocks are paired: they differ only at the order-th bit
static uint64
get_buddy(uint64 pa, int order)
{
  return pa ^ (PGSIZE << order);
}

// Check if a physical address is on the free list for given order
static int
is_on_free_list(uint64 pa, int order)
{
  struct run *r = kmem.free_lists[order].head;
  while (r) {
    if ((uint64)r == pa)
      return 1;
    r = r->next;
  }
  return 0;
}

// Remove a page from free list
static void
remove_from_list(uint64 pa, int order)
{
  struct run **pp = &kmem.free_lists[order].head;
  struct run *r = *pp;

  while (r) {
    if ((uint64)r == pa) {
      *pp = r->next;
      return;
    }
    pp = &r->next;
    r = r->next;
  }
}

// Initialize the buddy system allocator
void
kinit()
{
  int i;
  initlock(&kmem.lock, "kmem");
  for (i = 0; i < MAX_ORDER; i++) {
    kmem.free_lists[i].head = 0;
  }
  memset(&memstats, 0, sizeof(memstats));
  freerange(end, (void*)PHYSTOP);
  memstats.total_pages = memstats.free_pages;
}

// Initialize memory range using buddy system
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree(p);
    memstats.free_pages++;
  }
}

// Free memory block using buddy system
// Try to merge with buddy if possible
static void
buddy_free(uint64 pa, int order)
{
  uint64 buddy;

  while (order < MAX_ORDER - 1) {
    buddy = get_buddy(pa, order);

    // Check if buddy is free
    if (is_on_free_list(buddy, order)) {
      // Remove buddy from free list
      remove_from_list(buddy, order);

      // Merge: use the lower address as the new block
      if (buddy < pa)
        pa = buddy;

      order++;
      memstats.buddy_merges++;
    } else {
      break;
    }
  }

  // Add merged block to appropriate free list
  struct run *r = (struct run*)pa;
  r->next = kmem.free_lists[order].head;
  kmem.free_lists[order].head = r;
  memstats.free_pages++;
}

// Free the page of physical memory pointed at by pa
void
kfree(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs
  memset(pa, 1, PGSIZE);

  acquire(&kmem.lock);

  // Clear reference count for freed page
  int idx = pa2index((uint64)pa);
  if (idx >= 0 && idx < MAX_PAGES) {
    ref_count[idx] = 0;
  }

  // Add to free list (order 0 for single page)
  buddy_free((uint64)pa, 0);

  release(&kmem.lock);
}

// Allocate a block of 2^order pages
// Returns 0 if memory cannot be allocated
static void *
buddy_alloc(int order)
{
  struct run *r;
  int current_order;

  if (order < 0 || order >= MAX_ORDER)
    return 0;

  acquire(&kmem.lock);

  // Find the smallest free list that has a block
  for (current_order = order; current_order < MAX_ORDER; current_order++) {
    r = kmem.free_lists[current_order].head;
    if (r) {
      // Remove from free list
      kmem.free_lists[current_order].head = r->next;
      memstats.free_pages--;

      // Split the block until we reach the desired order
      while (current_order > order) {
        current_order--;
        uint64 buddy = get_buddy((uint64)r, current_order);
        struct run *buddy_run = (struct run*)buddy;

        // Add the higher half to free list
        buddy_run->next = kmem.free_lists[current_order].head;
        kmem.free_lists[current_order].head = buddy_run;
        memstats.buddy_splits++;
        memstats.free_pages++;
      }

      // Initialize reference count
      int idx = pa2index((uint64)r);
      if (idx >= 0 && idx < MAX_PAGES) {
        ref_count[idx] = 1;
      }

      release(&kmem.lock);
      return (void*)r;
    }
  }

  release(&kmem.lock);
  return 0;
}

// Allocate one 4096-byte page of physical memory
void *
kalloc(void)
{
  void *pa = buddy_alloc(0);  // order 0 = 1 page

  if (pa) {
    memset((char*)pa, 5, PGSIZE);
  }

  return pa;
}

// Increment COW fault counter
void
cow_fault_inc(void)
{
  acquire(&kmem.lock);
  memstats.cow_faults++;
  memstats.cow_copy_pages++;
  release(&kmem.lock);
}

// Increment lazy allocation counter
void
lazy_alloc_inc(void)
{
  acquire(&kmem.lock);
  memstats.lazy_allocs++;
  release(&kmem.lock);
}

// Fill memory statistics structure
void
fill_memstat(void *buf)
{
  acquire(&kmem.lock);
  *(struct memstats *)buf = memstats;
  release(&kmem.lock);
}
