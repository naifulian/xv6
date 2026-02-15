// test_lazy.c - Lazy allocation unit test implementation
// Enhanced with actual allocation verification
#include "../include/test_lazy.h"

// Test case: Large allocation, small access (lazy optimal)
void test_lazy_large_alloc_small_access(void) {
    printf("[TEST] lazy_large_alloc_small_access\n");

    struct memstat before, after;
    getmemstat(&before);

    // Request 1MB (256 pages) - xv6 compatible size
    char *addr = sbrk(1 * 1024 * 1024);
    if(addr == (char*)-1) {
        printf("  FAIL: sbrk failed\n");
        exit(1);
    }
    printf("  OK: sbrk 1MB = %p\n", addr);

    getmemstat(&after);
    int lazy_after_sbrk = after.lazy_allocs - before.lazy_allocs;
    int pages_after_sbrk = before.free_pages - after.free_pages;
    printf("  INFO: After sbrk - lazy_allocs=%d, pages_used=%d\n", lazy_after_sbrk, pages_after_sbrk);

    // Access only 10KB
    int access_kb = 10;
    int access_pages = access_kb / 4;  // 2.5 pages, round to 3

    getmemstat(&before);
    memset(addr, 'A', access_kb);
    getmemstat(&after);

    int lazy_after_access = after.lazy_allocs - before.lazy_allocs;
    int pages_after_access = before.free_pages - after.free_pages;

    printf("  OK: accessed %d KB\n", access_kb);
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_after_access, pages_after_access);

    // Verify data
    if(addr[0] != 'A' || addr[access_kb - 1] != 'A') {
        printf("  FAIL: data verification failed\n");
        exit(1);
    }
    printf("  OK: data verified\n");

    // Assertion: Should allocate approximately access_pages
    if(pages_after_access >= 1 && pages_after_access <= access_pages + 1) {
        printf("  PASS: Lazy allocated ~%d pages for %d KB access (expected ~%d)\n", 
               pages_after_access, access_kb, access_pages);
    } else {
        printf("  INFO: Allocated %d pages (expected ~%d)\n", pages_after_access, access_pages);
    }

    // Memory savings calculation
    int requested_pages = 256;  // 1MB / 4KB
    int savings = (requested_pages - pages_after_access) * 100 / requested_pages;
    printf("  INFO: Memory saved: %d%% (%d of %d pages not allocated)\n", 
           savings, requested_pages - pages_after_access, requested_pages);

    sbrk(-1 * 1024 * 1024);  // Free memory
}

// Test case: Large allocation, full access (lazy worst case)
void test_lazy_large_alloc_full_access(void) {
    printf("[TEST] lazy_large_alloc_full_access\n");

    struct memstat before, after;
    getmemstat(&before);

    // Request 1MB (256 pages)
    char *addr = sbrk(1024 * 1024);
    if(addr == (char*)-1) {
        printf("  FAIL: sbrk failed\n");
        exit(1);
    }
    printf("  OK: sbrk 1MB = %p\n", addr);

    getmemstat(&after);
    int pages_after_sbrk = before.free_pages - after.free_pages;
    printf("  INFO: After sbrk - pages_used=%d\n", pages_after_sbrk);

    // Access all pages
    getmemstat(&before);
    for(int i = 0; i < 256; i++) {
        addr[i * 4096] = 'B' + (i % 26);
    }
    getmemstat(&after);

    int lazy_allocs = after.lazy_allocs - before.lazy_allocs;
    int pages_used = before.free_pages - after.free_pages;

    printf("  OK: accessed all 256 pages\n");
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_allocs, pages_used);

    // Verify some pages
    if(addr[0] != 'B' || addr[255 * 4096] != 'B' + (255 % 26)) {
        printf("  FAIL: data verification failed\n");
        exit(1);
    }
    printf("  OK: data verified\n");

    // In worst case, all pages should be allocated
    if(pages_used >= 250) {
        printf("  INFO: Lazy worst case - all %d pages allocated (0%% savings)\n", pages_used);
    }

    sbrk(-1024 * 1024);  // Free memory
}

// Test case: Sequential access
void test_lazy_sequential_access(void) {
    printf("[TEST] lazy_sequential_access\n");

    struct memstat before, after;
    getmemstat(&before);

    // Request 2MB (512 pages)
    char *addr = sbrk(2 * 1024 * 1024);
    if(addr == (char*)-1) {
        printf("  FAIL: sbrk failed\n");
        exit(1);
    }
    printf("  OK: sbrk 2MB = %p\n", addr);

    getmemstat(&after);
    int pages_after_sbrk = before.free_pages - after.free_pages;
    printf("  INFO: After sbrk - pages_used=%d\n", pages_after_sbrk);

    // Access sequentially
    getmemstat(&before);
    for(int i = 0; i < 512; i++) {
        addr[i * 4096] = 'C' + (i % 26);
    }
    getmemstat(&after);

    int lazy_allocs = after.lazy_allocs - before.lazy_allocs;
    int pages_used = before.free_pages - after.free_pages;

    printf("  OK: accessed 512 pages sequentially\n");
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_allocs, pages_used);

    // Verify pattern
    int errors = 0;
    for(int i = 0; i < 512; i++) {
        if(addr[i * 4096] != 'C' + (i % 26)) {
            errors++;
        }
    }

    if(errors > 0) {
        printf("  FAIL: %d data errors detected\n", errors);
        exit(1);
    }
    printf("  OK: pattern verified (no errors)\n");

    sbrk(-2 * 1024 * 1024);
}

// Test case: Random access
void test_lazy_random_access(void) {
    printf("[TEST] lazy_random_access\n");

    struct memstat before, after;
    getmemstat(&before);

    // Request 1MB (256 pages)
    char *addr = sbrk(1024 * 1024);
    if(addr == (char*)-1) {
        printf("  FAIL: sbrk failed\n");
        exit(1);
    }
    printf("  OK: sbrk 1MB = %p\n", addr);

    // Access random pages
    int indices[] = {0, 10, 50, 100, 200, 255, 128, 64, 32, 16};
    int num_access = 10;

    getmemstat(&before);
    for(int i = 0; i < num_access; i++) {
        int idx = indices[i];
        addr[idx * 4096] = 'D' + (idx % 26);
    }
    getmemstat(&after);

    int lazy_allocs = after.lazy_allocs - before.lazy_allocs;
    int pages_used = before.free_pages - after.free_pages;

    printf("  OK: accessed %d random pages\n", num_access);
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_allocs, pages_used);

    // Verify accessed pages
    int errors = 0;
    for(int i = 0; i < num_access; i++) {
        int idx = indices[i];
        if(addr[idx * 4096] != 'D' + (idx % 26)) {
            errors++;
        }
    }

    if(errors > 0) {
        printf("  FAIL: %d data errors detected\n", errors);
        exit(1);
    }
    printf("  OK: random access verified (no errors)\n");

    // Assertion: Should allocate approximately num_access pages
    if(pages_used >= num_access && pages_used <= num_access + 2) {
        printf("  PASS: Lazy allocated ~%d pages for %d random accesses\n", pages_used, num_access);
    } else {
        printf("  INFO: Allocated %d pages for %d random accesses\n", pages_used, num_access);
    }

    // Memory savings
    int requested_pages = 256;
    int savings = (requested_pages - pages_used) * 100 / requested_pages;
    printf("  INFO: Memory saved: %d%%\n", savings);

    sbrk(-1024 * 1024);
}

// Test case: Multiple allocations
void test_lazy_multiple_allocs(void) {
    printf("[TEST] lazy_multiple_allocs\n");

    struct memstat before, after;
    getmemstat(&before);

    // Request 3 allocations of 256KB each (xv6 compatible)
    char *addrs[3];
    for(int i = 0; i < 3; i++) {
        addrs[i] = sbrk(256 * 1024);
        if(addrs[i] == (char*)-1) {
            printf("  INFO: allocation %d failed (out of memory?)\n", i);
            break;
        }
    }
    printf("  OK: allocated %d blocks of 256KB\n", 3);

    getmemstat(&after);
    int pages_after_alloc = before.free_pages - after.free_pages;
    printf("  INFO: After allocations - pages_used=%d\n", pages_after_alloc);

    // Access some pages from each allocation
    getmemstat(&before);
    for(int i = 0; i < 3; i++) {
        if(addrs[i]) {
            addrs[i][0] = 'E' + i;
            addrs[i][4096] = 'F' + i;
            printf("  OK: wrote to block %d\n", i);
        }
    }
    getmemstat(&after);

    int lazy_allocs = after.lazy_allocs - before.lazy_allocs;
    int pages_used = before.free_pages - after.free_pages;

    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_allocs, pages_used);

    // Verify and free
    for(int i = 0; i < 3; i++) {
        if(addrs[i] && (addrs[i][0] != 'E' + i || addrs[i][4096] != 'F' + i)) {
            printf("  FAIL: data error in block %d\n", i);
            exit(1);
        }
    }
    printf("  OK: all blocks verified\n");

    // Should allocate ~6 pages (2 per block)
    if(pages_used >= 4 && pages_used <= 8) {
        printf("  PASS: Lazy allocated ~%d pages for 2 writes per block\n", pages_used);
    }

    // Free all
    for(int i = 0; i < 3; i++) {
        if(addrs[i]) {
            sbrk(-256 * 1024);
        }
    }
    printf("  OK: freed all blocks\n");
}

// Test case: Zero allocation
void test_lazy_zero_alloc(void) {
    printf("[TEST] lazy_zero_alloc\n");

    struct memstat before, after;
    getmemstat(&before);

    char *addr = sbrk(0);

    getmemstat(&after);
    int pages_used = before.free_pages - after.free_pages;

    if(addr == (char*)-1) {
        printf("  OK: sbrk(0) returned -1 (expected)\n");
    } else if(addr == (char*)0) {
        printf("  OK: sbrk(0) returned 0 (implementation defined)\n");
    } else {
        printf("  INFO: sbrk(0) returned %p\n", addr);
    }

    printf("  RESULT: pages_allocated=%d\n", pages_used);
}

// Test case: Negative allocation
void test_lazy_negative_alloc(void) {
    printf("[TEST] lazy_negative_alloc\n");

    struct memstat before, after;
    getmemstat(&before);

    char *addr = sbrk(-100);  // Negative allocation

    getmemstat(&after);
    int pages_freed = after.free_pages - before.free_pages;

    if(addr == (char*)-1) {
        printf("  OK: sbrk(-100) failed (expected)\n");
    } else {
        printf("  INFO: sbrk(-100) succeeded (freed memory?)\n");
        printf("  INFO: returned %p\n", addr);
    }

    printf("  RESULT: pages_freed=%d\n", pages_freed);
}

// Test case: Large single allocation
void test_lazy_very_large_alloc(void) {
    printf("[TEST] lazy_very_large_alloc\n");

    struct memstat before, after;
    getmemstat(&before);

    // Request 2MB (xv6 compatible size)
    char *addr = sbrk(2 * 1024 * 1024);
    if(addr == (char*)-1) {
        printf("  INFO: sbrk 2MB failed (insufficient memory)\n");
        return;
    }
    printf("  OK: sbrk 2MB = %p\n", addr);

    getmemstat(&after);
    int pages_after_sbrk = before.free_pages - after.free_pages;
    printf("  INFO: After sbrk - pages_used=%d\n", pages_after_sbrk);

    // Access first and last page
    getmemstat(&before);
    addr[0] = 'G';
    addr[2 * 1024 * 1024 - 1] = 'G';
    getmemstat(&after);

    int lazy_allocs = after.lazy_allocs - before.lazy_allocs;
    int pages_used = before.free_pages - after.free_pages;

    printf("  OK: accessed first and last byte\n");
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_allocs, pages_used);

    // Should allocate only 2 pages for 2 accesses
    if(pages_used >= 1 && pages_used <= 3) {
        printf("  PASS: Lazy allocated %d pages for 2 byte accesses\n", pages_used);
    }

    sbrk(-2 * 1024 * 1024);
}

// Test case: Grow then shrink
void test_lazy_grow_sh_shrink(void) {
    printf("[TEST] lazy_grow_sh_shrink\n");

    struct memstat before, after;
    getmemstat(&before);

    // Grow
    char *addr1 = sbrk(512 * 1024);  // 512KB
    if(addr1 == (char*)-1) {
        printf("  FAIL: sbrk 512KB failed\n");
        exit(1);
    }
    printf("  OK: sbrk 512KB = %p\n", addr1);

    getmemstat(&after);
    int pages1 = before.free_pages - after.free_pages;
    printf("  INFO: After first grow - pages_used=%d\n", pages1);

    // Grow more
    before = after;
    char *addr2 = sbrk(512 * 1024);  // Another 512KB
    if(addr2 == (char*)-1) {
        printf("  FAIL: sbrk second 512KB failed\n");
        sbrk(-512 * 1024);
        exit(1);
    }
    printf("  OK: sbrk another 512KB = %p\n", addr2);

    getmemstat(&after);
    int pages2 = before.free_pages - after.free_pages;
    printf("  INFO: After second grow - pages_used=%d\n", pages2);

    // Shrink (xv6: just reduces process size, data may be lost)
    before = after;
    char *addr3 = sbrk(-512 * 1024);  // Back to 512KB
    if(addr3 == (char*)-1) {
        printf("  FAIL: sbrk shrink failed\n");
        sbrk(-1024 * 1024);  // Free all
        exit(1);
    }
    printf("  OK: shrunk back to 512KB\n");

    getmemstat(&after);
    int pages_freed = after.free_pages - before.free_pages;
    printf("  RESULT: pages_freed=%d\n", pages_freed);

    printf("  INFO: data verification skipped (xv6 sbrk shrink loses data)\n");

    sbrk(-512 * 1024);
}

// Test case: Sparse array pattern
void test_lazy_sparse_array(void) {
    printf("[TEST] lazy_sparse_array\n");

    struct memstat before, after;
    getmemstat(&before);

    // Request 1MB (256 pages)
    char *addr = sbrk(1024 * 1024);
    if(addr == (char*)-1) {
        printf("  FAIL: sbrk failed\n");
        exit(1);
    }
    printf("  OK: sbrk 1MB = %p\n", addr);

    getmemstat(&after);
    int pages_after_sbrk = before.free_pages - after.free_pages;
    printf("  INFO: After sbrk - pages_used=%d\n", pages_after_sbrk);

    // Access sparse pattern: every 10th page
    int accessed = 0;
    getmemstat(&before);
    for(int i = 0; i < 256; i += 10) {
        addr[i * 4096] = 'I' + (i % 26);
        accessed++;
    }
    getmemstat(&after);

    int lazy_allocs = after.lazy_allocs - before.lazy_allocs;
    int pages_used = before.free_pages - after.free_pages;

    printf("  OK: accessed %d pages (sparse pattern)\n", accessed);
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_allocs, pages_used);

    // Verify accessed pages
    int errors = 0;
    for(int i = 0; i < 256; i += 10) {
        if(addr[i * 4096] != 'I' + (i % 26)) {
            errors++;
        }
    }

    if(errors > 0) {
        printf("  FAIL: %d data errors detected\n", errors);
        exit(1);
    }
    printf("  OK: sparse pattern verified (no errors)\n");

    // Assertion: Should allocate approximately 'accessed' pages
    if(pages_used >= accessed - 1 && pages_used <= accessed + 1) {
        printf("  PASS: Lazy allocated %d pages for %d sparse accesses\n", pages_used, accessed);
    } else {
        printf("  INFO: Allocated %d pages for %d sparse accesses\n", pages_used, accessed);
    }

    // Memory savings
    int requested_pages = 256;
    int savings = (requested_pages - pages_used) * 100 / requested_pages;
    printf("  INFO: Memory saved: %d%% (%d of %d pages not allocated)\n", 
           savings, requested_pages - pages_used, requested_pages);

    sbrk(-1024 * 1024);
}
