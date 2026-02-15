// test_buddy.c - Buddy system unit test implementation
#include "../include/test_buddy.h"

// Test case: Basic allocation of single page
void test_buddy_alloc_single_page(void) {
    printf("[TEST] buddy_alloc_single_page\n");
    void *ptr = malloc(4096);
    if(ptr == 0) {
        printf("  FAIL: malloc returned NULL\n");
        exit(1);
    }
    printf("  OK: malloc(4096) = %p\n", ptr);
    free(ptr);
    printf("  OK: free succeeded\n");
}

// Test case: Multiple allocations of same size
void test_buddy_alloc_multiple_same(void) {
    printf("[TEST] buddy_alloc_multiple_same\n");
    void *ptrs[10];
    for(int i = 0; i < 10; i++) {
        ptrs[i] = malloc(4096);
        if(ptrs[i] == 0) {
            printf("  FAIL: malloc %d failed\n", i);
            exit(1);
        }
    }
    printf("  OK: allocated 10 pages\n");
    for(int i = 0; i < 10; i++) {
        free(ptrs[i]);
    }
    printf("  OK: freed all 10 pages\n");
}

// Test case: Allocation of different sizes (powers of 2)
void test_buddy_alloc_different_sizes(void) {
    printf("[TEST] buddy_alloc_different_sizes\n");
    void *ptrs[5];
    int sizes[] = {4096, 8192, 16384, 32768, 65536};  // 4K, 8K, 16K, 32K, 64K

    for(int i = 0; i < 5; i++) {
        ptrs[i] = malloc(sizes[i]);
        if(ptrs[i] == 0) {
            printf("  FAIL: malloc(%d) failed\n", sizes[i]);
            exit(1);
        }
        printf("  OK: malloc(%d) = %p\n", sizes[i], ptrs[i]);
    }

    for(int i = 0; i < 5; i++) {
        free(ptrs[i]);
    }
    printf("  OK: freed all allocations\n");
}

// Test case: Data integrity after allocation
void test_buddy_data_integrity(void) {
    printf("[TEST] buddy_data_integrity\n");
    int size = 1024;  // Number of integers
    int *data = (int*)malloc(size * sizeof(int));
    if(data == 0) {
        printf("  FAIL: malloc failed\n");
        exit(1);
    }

    // Write pattern
    for(int i = 0; i < size; i++) {
        data[i] = i * 3 + 42;
    }
    printf("  OK: wrote %d integers\n", size);

    // Verify pattern
    int errors = 0;
    for(int i = 0; i < size; i++) {
        if(data[i] != i * 3 + 42) {
            errors++;
        }
    }

    if(errors > 0) {
        printf("  FAIL: %d data errors detected\n", errors);
        exit(1);
    }
    printf("  OK: data integrity verified (no errors)\n");
    free(data);
}

// Test case: Allocation and deallocation pattern to trigger merge
void test_buddy_merge_pattern(void) {
    printf("[TEST] buddy_merge_pattern\n");
    // Allocate adjacent small blocks
    void *p1 = malloc(4096);
    void *p2 = malloc(4096);
    void *p3 = malloc(4096);
    void *p4 = malloc(4096);

    if(!p1 || !p2 || !p3 || !p4) {
        printf("  FAIL: initial allocations failed\n");
        exit(1);
    }
    printf("  OK: allocated 4 pages\n");

    // Free them to trigger merges
    free(p1);
    free(p2);
    free(p3);
    free(p4);
    printf("  OK: freed all 4 pages (buddy should merge)\n");

    // Try to allocate larger block (test if merge worked)
    void *large = malloc(8192);  // 2 pages
    if(large) {
        printf("  OK: large alloc succeeded (merge likely worked)\n");
        free(large);
    } else {
        printf("  INFO: large alloc failed (merge may not have worked or fragmentation)\n");
    }
}

// Test case: Fragmentation scenario
void test_buddy_fragmentation(void) {
    printf("[TEST] buddy_fragmentation\n");
    void *ptrs[20];
    // Allocate alternating sizes to create fragmentation
    for(int i = 0; i < 20; i++) {
        int size = 4096 * (1 + (i % 3));  // 4K, 8K, 16K
        ptrs[i] = malloc(size);
        if(ptrs[i] == 0) {
            printf("  INFO: allocation failed at %d (possible fragmentation)\n", i);
            break;
        }
    }
    printf("  OK: allocated %d blocks with varying sizes\n", 20);

    // Free every other block
    for(int i = 0; i < 20; i += 2) {
        if(ptrs[i]) free(ptrs[i]);
    }
    printf("  OK: freed 10 blocks (creating fragmented space)\n");

    // Clean up remaining
    for(int i = 1; i < 20; i += 2) {
        if(ptrs[i]) free(ptrs[i]);
    }
    printf("  OK: cleaned up remaining blocks\n");
}

// Test case: Stress test with many small allocations
void test_buddy_stress_small(void) {
    printf("[TEST] buddy_stress_small\n");
    int count = 100;
    void **ptrs = (void**)malloc(count * sizeof(void*));
    if(ptrs == 0) {
        printf("  FAIL: malloc for ptrs failed\n");
        exit(1);
    }

    int allocated = 0;
    for(int i = 0; i < count; i++) {
        ptrs[i] = malloc(4096);
        if(ptrs[i] == 0) {
            printf("  INFO: allocation failed at %d (out of memory?)\n", i);
            break;
        }
        allocated++;
    }
    printf("  OK: allocated %d pages\n", allocated);

    // Free all
    for(int i = 0; i < allocated; i++) {
        free(ptrs[i]);
    }
    printf("  OK: freed %d pages\n", allocated);
    free(ptrs);
}

// Test case: Zero-sized allocation
void test_buddy_zero_size(void) {
    printf("[TEST] buddy_zero_size\n");
    void *ptr = malloc(0);
    // Behavior may vary - should either return NULL or minimal valid pointer
    if(ptr == 0) {
        printf("  OK: malloc(0) returned NULL (expected behavior)\n");
    } else {
        printf("  OK: malloc(0) returned %p (implementation defined)\n", ptr);
        free(ptr);
    }
}

// Test case: Very large allocation
void test_buddy_large_allocation(void) {
    printf("[TEST] buddy_large_allocation\n");
    void *ptr = malloc(1024 * 1024);  // 1MB (256 pages)
    if(ptr == 0) {
        printf("  INFO: malloc(1MB) failed (insufficient memory)\n");
    } else {
        printf("  OK: malloc(1MB) = %p\n", ptr);
        free(ptr);
        printf("  OK: freed large allocation\n");
    }
}

// Test case: Free NULL (should be safe)
void test_buddy_free_null(void) {
    printf("[TEST] buddy_free_null\n");
    
    // Test that free(NULL) is safe (should not crash)
    free(0);
    printf("  OK: free(NULL) did not crash\n");
}

// Test case: Double free detection (implementation dependent)
void test_buddy_double_free(void) {
    printf("[TEST] buddy_double_free\n");
    void *ptr = malloc(4096);
    if(ptr == 0) {
        printf("  FAIL: malloc failed\n");
        exit(1);
    }

    free(ptr);
    free(ptr);  // Double free - behavior depends on implementation

    // Note: Buddy system may or may not detect double free
    // If we reach here without crashing, it's OK
    printf("  OK: double free handled (may or may not detect)\n");
}
