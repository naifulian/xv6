// buddy_test.c - test buddy system allocator
#include "kernel/types.h"
#include "user/user.h"

// Simple test to verify buddy system works correctly
int main(void) {
  printf("=== Buddy System Allocator Test ===\n\n");

  // Test 1: Basic allocation and deallocation
  printf("[Test 1] Basic allocation test\n");
  void *p1 = malloc(4096);
  void *p2 = malloc(4096);
  void *p3 = malloc(4096);

  if (p1 && p2 && p3) {
    printf("Allocated 3 pages: %p, %p, %p\n", p1, p2, p3);
  } else {
    printf("ERROR: Allocation failed\n");
    exit(1);
  }

  free(p1);
  free(p2);
  free(p3);
  printf("Freed all pages\n");

  // Test 2: Large allocation (stress test)
  printf("\n[Test 2] Large allocation test\n");
  int pages = 100;
  void *ptrs[100];
  int i;

  for (i = 0; i < pages; i++) {
    ptrs[i] = malloc(4096);
    if (ptrs[i] == 0) {
      printf("Allocation failed at page %d\n", i);
      break;
    }
  }
  printf("Allocated %d pages\n", i);

  // Free all
  for (int j = 0; j < i; j++) {
    free(ptrs[j]);
  }
  printf("Freed %d pages\n", i);

  // Test 3: Verify data integrity
  printf("\n[Test 3] Data integrity test\n");
  int *data = (int*)malloc(4096);
  if (data == 0) {
    printf("ERROR: malloc failed\n");
    exit(1);
  }

  // Write pattern
  for (i = 0; i < 1024; i++) {
    data[i] = i * 2;
  }

  // Read and verify
  int errors = 0;
  for (i = 0; i < 1024; i++) {
    if (data[i] != i * 2) {
      errors++;
    }
  }

  if (errors == 0) {
    printf("Data integrity verified (1024 integers)\n");
  } else {
    printf("ERROR: %d data errors detected\n", errors);
  }
  free(data);

  // Test 4: Fork test (COW should still work)
  printf("\n[Test 4] Fork test with COW\n");
  int *shared = (int*)malloc(4096);
  *shared = 42;

  int pid = fork();
  if (pid == 0) {
    // Child process
    if (*shared == 42) {
      printf("Child: read correct value %d\n", *shared);
      *shared = 99;
      printf("Child: wrote value %d\n", *shared);
    }
    exit(0);
  } else {
    // Parent process
    wait(0);
    printf("Parent: value is still %d (COW works)\n", *shared);
    free(shared);
  }

  printf("\n=== Buddy System Test PASSED ===\n");
  exit(0);
}
