// mmap_test.c - test program for mmap system call
#include "kernel/types.h"
#include "user/user.h"

// mmap flags
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20

// mmap protection flags
#define PROT_READ 0x1
#define PROT_WRITE 0x2

int main(void) {
  printf("=== mmap test program ===\n");

  // Test 1: Basic anonymous mmap
  printf("\n[Test 1] Basic anonymous mmap\n");
  uint length = 4096;  // One page
  void *addr = mmap(0, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (addr == (void*)-1) {
    printf("mmap failed\n");
    exit(1);
  }

  printf("mmap returned address: %p\n", addr);

  // Test 2: Write to mapped memory
  printf("\n[Test 2] Write to mapped memory\n");
  int *data = (int*)addr;
  for (int i = 0; i < 16; i++) {
    data[i] = i * 100;
  }
  printf("Wrote data to mapped region\n");

  // Test 3: Read back from mapped memory
  printf("\n[Test 3] Read back from mapped memory\n");
  int errors = 0;
  for (int i = 0; i < 16; i++) {
    if (data[i] != i * 100) {
      printf("Error at index %d: expected %d, got %d\n", i, i * 100, data[i]);
      errors++;
    }
  }
  if (errors == 0) {
    printf("All data read back correctly\n");
  } else {
    printf("Found %d errors\n", errors);
  }

  // Test 4: Test munmap by allocating a new region and unmapping it
  printf("\n[Test 4] Test munmap on new region\n");
  void *addr2 = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (addr2 == (void*)-1) {
    printf("Second mmap failed\n");
    exit(1);
  }
  printf("Second mmap returned address: %p\n", addr2);

  // Try to munmap the second region (which should be at the end)
  int ret = munmap(addr2, 4096);
  if (ret == 0) {
    printf("munmap of second region succeeded\n");
  } else {
    printf("munmap of second region failed\n");
  }

  printf("\n=== mmap basic tests completed ===\n");
  exit(0);
}
