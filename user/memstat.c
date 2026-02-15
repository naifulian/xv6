// memstat.c - Memory statistics display tool for xv6

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
print_bar(int value, int max, int width)
{
  int filled = 0;
  if(max > 0) {
    filled = value * width / max;
  }
  printf("[");
  for(int i = 0; i < width; i++) {
    if(i < filled) {
      printf("#");
    } else {
      printf(" ");
    }
  }
  printf("]");
}

int
main(int argc, char *argv[])
{
  struct memstat ms;
  int continuous = 0;
  int interval = 1;

  for(int i = 1; i < argc; i++) {
    if(strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
      continuous = 1;
      interval = atoi(argv[++i]);
      if(interval < 1) interval = 1;
    } else if(strcmp(argv[i], "-h") == 0) {
      printf("Usage: memstat [-c <interval>]\n");
      printf("  -c  Continuous display with interval (ticks)\n");
      printf("\nMemory Statistics:\n");
      printf("  total_pages   - Total physical pages\n");
      printf("  free_pages    - Available pages\n");
      printf("  buddy_merges  - Buddy system merge operations\n");
      printf("  buddy_splits  - Buddy system split operations\n");
      printf("  cow_faults    - COW page fault count\n");
      printf("  lazy_allocs   - Lazy allocation count\n");
      printf("  cow_copy_pages - Pages copied due to COW\n");
      exit(0);
    }
  }

  do {
    if(getmemstat(&ms) < 0) {
      printf("memstat: failed to get memory statistics\n");
      exit(1);
    }

    printf("\n=== Memory Statistics ===\n\n");

    printf("Physical Memory:\n");
    printf("  Total pages:  %d (%d KB)\n", ms.total_pages, ms.total_pages * 4);
    printf("  Free pages:   %d (%d KB)\n", ms.free_pages, ms.free_pages * 4);
    printf("  Used pages:   %d (%d KB)\n", 
           ms.total_pages - ms.free_pages, 
           (ms.total_pages - ms.free_pages) * 4);
    printf("  Usage:        ");
    print_bar(ms.total_pages - ms.free_pages, ms.total_pages, 30);
    printf(" %d%%\n", 
           ms.total_pages > 0 ? (ms.total_pages - ms.free_pages) * 100 / ms.total_pages : 0);

    printf("\nBuddy System:\n");
    printf("  Merges:       %d\n", ms.buddy_merges);
    printf("  Splits:       %d\n", ms.buddy_splits);
    if(ms.buddy_merges + ms.buddy_splits > 0) {
      printf("  Merge ratio:  %d%%\n", 
             ms.buddy_merges * 100 / (ms.buddy_merges + ms.buddy_splits));
    }

    printf("\nCOW (Copy-on-Write):\n");
    printf("  COW faults:   %d\n", ms.cow_faults);
    printf("  Pages copied: %d\n", ms.cow_copy_pages);

    printf("\nLazy Allocation:\n");
    printf("  Lazy allocs:  %d\n", ms.lazy_allocs);
    printf("  Memory saved: ~%d KB (pages not actually allocated)\n", 
           ms.lazy_allocs * 4);

    if(continuous) {
      printf("\n--- Press Ctrl+C to stop ---\n");
      sleep(interval);
    }
  } while(continuous);

  exit(0);
}
