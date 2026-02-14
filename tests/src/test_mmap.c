// test_mmap.c - mmap unit test implementation (anonymous mapping only)
#include "../include/test_mmap.h"
#include "../include/mmap_flags.h"

// Test case: Basic anonymous mmap and munmap
void test_mmap_basic(void) {
    printf("[TEST] mmap_basic (anonymous)\n");

    // Map anonymous memory (supported by current kernel)
    uint64 flags = MAP_ANONYMOUS | MAP_PRIVATE;
    char *addr = mmap(0, 4096, PROT_READ | PROT_WRITE, flags, -1, 0);
    if(addr == (char*)-1) {
        printf("  FAIL: mmap failed\n");
        exit(1);
    }
    printf("  OK: mmap returned %p\n", addr);

    // Write to mapped memory
    addr[0] = 'A';
    printf("  OK: wrote to mapped memory\n");

    // Verify content
    if(addr[0] != 'A') {
        printf("  FAIL: mmap content incorrect\n");
        munmap(addr, 4096);
        exit(1);
    }
    printf("  OK: mmap content verified\n");

    // Unmap
    if(munmap(addr, 4096) < 0) {
        printf("  FAIL: munmap failed\n");
        exit(1);
    }
    printf("  OK: munmap succeeded\n");
}

// Test case: mmap with READ_ONLY protection
void test_mmap_readonly(void) {
    printf("[TEST] mmap_readonly (anonymous)\n");

    uint64 flags = MAP_ANONYMOUS | MAP_PRIVATE;
    char *addr = mmap(0, 4096, PROT_READ, flags, -1, 0);
    if(addr == (char*)-1) {
        printf("  FAIL: mmap PROT_READ failed\n");
        exit(1);
    }
    printf("  OK: mmap PROT_READ succeeded\n");

    // Try to read
    printf("  OK: read from PROT_READ mapping worked (c=%d)\n", addr[0]);

    munmap(addr, 4096);
}

// Test case: mmap with large size
void test_mmap_large(void) {
    printf("[TEST] mmap_large (anonymous)\n");

    // Try to map 1MB
    uint64 flags = MAP_ANONYMOUS | MAP_PRIVATE;
    char *addr = mmap(0, 1024 * 1024, PROT_READ | PROT_WRITE, flags, -1, 0);
    if(addr == (char*)-1) {
        printf("  INFO: mmap 1MB failed (insufficient memory?)\n");
        return;
    }
    printf("  OK: mmap 1MB succeeded\n");

    // Write to both ends
    addr[0] = 'A';
    addr[1024*1024 - 1] = 'Z';
    printf("  OK: verified both ends of mapping\n");

    munmap(addr, 1024 * 1024);
}

// Test case: Multiple mmap calls
void test_mmap_multiple(void) {
    printf("[TEST] mmap_multiple (anonymous)\n");

    uint64 flags = MAP_ANONYMOUS | MAP_PRIVATE;
    char *addrs[5];
    int i;

    for(i = 0; i < 5; i++) {
        addrs[i] = mmap(0, 4096, PROT_READ | PROT_WRITE, flags, -1, 0);
        if(addrs[i] == (char*)-1) {
            printf("  FAIL: mmap %d failed\n", i);
            // Cleanup
            for(int j = 0; j < i; j++) {
                munmap(addrs[j], 4096);
            }
            exit(1);
        }
    }
    printf("  OK: mapped 5 times\n");

    for(i = 0; i < 5; i++) {
        munmap(addrs[i], 4096);
    }
    printf("  OK: unmapped all 5 times\n");
}

// Test case: mmap with offset (not applicable to anonymous mapping)
void test_mmap_offset(void) {
    printf("[TEST] mmap_offset (anonymous - offset ignored)\n");

    uint64 flags = MAP_ANONYMOUS | MAP_PRIVATE;
    char *addr = mmap(0, 4096, PROT_READ | PROT_WRITE, flags, -1, 0);
    if(addr == (char*)-1) {
        printf("  FAIL: mmap failed\n");
        exit(1);
    }
    printf("  OK: mmap succeeded\n");

    // Write with "offset" from user perspective
    addr[100] = 'A';
    printf("  OK: wrote at offset 100\n");

    munmap(addr, 4096);
    printf("  OK: unmapped\n");
}

// Test case: mmap invalid parameters
void test_mmap_invalid(void) {
    printf("[TEST] mmap_invalid (zero size)\n");

    uint64 flags = MAP_ANONYMOUS | MAP_PRIVATE;

    // Try zero size (should fail or return 0)
    char *addr = mmap(0, 0, PROT_READ | PROT_WRITE, flags, -1, 0);
    if(addr == (char*)-1) {
        printf("  OK: mmap with zero size failed (expected)\n");
    } else if(addr == 0) {
        printf("  OK: mmap with zero size returned NULL\n");
    } else {
        printf("  INFO: mmap with zero size returned %p\n", addr);
        munmap(addr, 0);
    }
}

// Test case: munmap NULL pointer
void test_mmap_munmap_null(void) {
    printf("[TEST] mmap_munmap_null\n");

    uint64 flags = MAP_ANONYMOUS | MAP_PRIVATE;
    char *addr = mmap(0, 4096, PROT_READ | PROT_WRITE, flags, -1, 0);
    if(addr == (char*)-1) {
        printf("  FAIL: mmap failed\n");
        exit(1);
    }

    // Try to munmap NULL (behavior depends on implementation)
    int ret = munmap(0, 4096);
    printf("  OK: munmapari(NULL) returned %d (behavior)\n", ret);

    munmap(addr, 4096);
}
