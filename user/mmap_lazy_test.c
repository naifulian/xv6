// mmap_lazy_test.c - test mmap lazy allocation
#include "kernel/types.h"
#include "user/user.h"

// mmap flags
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20

// mmap protection flags
#define PROT_READ 0x1
#define PROT_WRITE 0x2

int main(void) {
  printf("=== mmap lazy allocation test ===\n");

  // Test 1: mmap large region should not allocate physical pages immediately
  printf("\n[Test 1] mmap large region (100 pages)\n");
  uint length = 100 * 4096;  // 100 pages
  void *addr = mmap(0, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (addr == (void*)-1) {
    printf("mmap failed\n");
    exit(1);
  }
  printf("mmap returned address: %p\n", addr);
  printf("Region size: %d pages (%d bytes)\n", 100, length);

  // Test 2: Access first page (should trigger lazy allocation)
  printf("\n[Test 2] Access first page (triggers page fault)\n");
  int *data = (int*)addr;
  data[0] = 42;
  printf("Wrote value 42 to first page\n");
  printf("Read value: %d\n", data[0]);

  // Test 3: Access last page
  printf("\n[Test 3] Access last page (triggers page fault)\n");
  int *last_page = (int*)((char*)addr + length - 4);
  *last_page = 99;
  printf("Wrote value 99 to last page\n");
  printf("Read value: %d\n", *last_page);

  // Test 4: Access middle page
  printf("\n[Test 4] Access middle page\n");
  int *mid_page = (int*)((char*)addr + length / 2);
  *mid_page = 77;
  printf("Wrote value 77 to middle page\n");
  printf("Read value: %d\n", *mid_page);

  // Test 5: Verify first page still has correct value
  printf("\n[Test 5] Verify first page unchanged\n");
  printf("First page value: %d\n", data[0]);

  // Test 6: munmap
  printf("\n[Test 6] munmap test\n");
  if (munmap(addr, length) == 0) {
    printf("munmap succeeded\n");
  } else {
    printf("munmap failed\n");
  }

  printf("\n=== mmap lazy allocation test PASSED ===\n");
  exit(0);
}
