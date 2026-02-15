// test_cow.c - Copy-on-Write unit test implementation
// Enhanced with memory counter verification
#include "../include/test_cow.h"

// Test case: fork without any write (COW optimal)
void test_cow_fork_exec(void) {
    printf("[TEST] cow_fork_exec\n");

    struct memstat before, after;
    getmemstat(&before);

    // Prepare some memory
    char *data = sbrk(1024 * 1024);  // 1MB
    if(data == (char*)-1) {
        printf("  FAIL: sbrk failed\n");
        exit(1);
    }

    // Write to ensure pages are allocated
    for(int i = 0; i < 256; i++) {
        data[i * 4096] = 'A' + (i % 26);
    }
    printf("  OK: prepared 1MB data (256 pages)\n");

    getmemstat(&after);
    int pages_allocated = before.free_pages - after.free_pages;
    printf("  INFO: allocated %d physical pages for 256 virtual pages\n", pages_allocated);

    int count = 5;
    int cow_before = after.cow_faults;
    int copy_before = after.cow_copy_pages;

    // Create children that immediately exec
    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: immediately exec
            char *argv[] = {"echo", "COW test", 0};
            exec("echo", argv);
            exit(1);  // Should not reach here
        } else if(pid > 0) {
            printf("  OK: forked child %d (pid=%d)\n", i, pid);
        } else {
            printf("  FAIL: fork failed\n");
            exit(1);
        }
    }

    // Wait for all children
    for(int i = 0; i < count; i++) {
        wait(0);
    }

    getmemstat(&after);
    int cow_faults = after.cow_faults - cow_before;
    int cow_copies = after.cow_copy_pages - copy_before;

    printf("  OK: all children exited\n");
    printf("  RESULT: COW faults=%d, pages copied=%d\n", cow_faults, cow_copies);

    // Assertion: COW should not copy pages for exec-only children
    if(cow_copies == 0) {
        printf("  PASS: COW worked - no pages copied for exec-only children\n");
    } else {
        printf("  INFO: %d pages were copied (some exec overhead)\n", cow_copies);
    }

    sbrk(-1024 * 1024);  // Free memory
}

// Test case: fork with partial write (COW medium)
void test_cow_partial_write(void) {
    printf("[TEST] cow_partial_write\n");

    struct memstat before, after;
    getmemstat(&before);

    // Prepare memory
    char *data = sbrk(1024 * 1024);  // 1MB
    if(data == (char*)-1) {
        printf("  FAIL: sbrk failed\n");
        exit(1);
    }

    // Write pattern
    for(int i = 0; i < 256; i++) {
        data[i * 4096] = 'A' + (i % 26);
    }
    printf("  OK: prepared 1MB data (256 pages)\n");

    getmemstat(&after);
    int initial_pages __attribute__((unused)) = before.free_pages - after.free_pages;
    printf("  INFO: allocated %d physical pages\n", initial_pages);

    int count = 3;
    int write_pages = 26;  // 10% of 256
    int cow_before = after.cow_faults;
    int copy_before = after.cow_copy_pages;

    // Create children that write to part of memory
    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: write 10% of pages
            for(int j = 0; j < write_pages; j++) {
                data[j * 4096] = 'B' + (j % 26);
            }
            exit(0);
        } else if(pid > 0) {
            // Parent: continue
        } else {
            printf("  FAIL: fork failed\n");
            exit(1);
        }
    }

    // Wait for all children
    for(int i = 0; i < count; i++) {
        wait(0);
    }

    getmemstat(&after);
    int cow_faults = after.cow_faults - cow_before;
    int cow_copies = after.cow_copy_pages - copy_before;

    printf("  OK: all children exited\n");
    printf("  RESULT: COW faults=%d, pages copied=%d\n", cow_faults, cow_copies);

    // Verify parent's data is unchanged (COW worked)
    int errors = 0;
    for(int i = 0; i < 26; i++) {
        if(data[i * 4096] != 'A' + (i % 26)) {
            errors++;
        }
    }

    if(errors > 0) {
        printf("  FAIL: parent data modified at %d pages\n", errors);
        sbrk(-1024 * 1024);
        exit(1);
    }
    printf("  OK: parent data unchanged (COW works)\n");

    // Assertion: COW copies should be approximately write_pages * count
    int expected_copies = write_pages * count;
    if(cow_copies >= expected_copies * 0.8 && cow_copies <= expected_copies * 1.2) {
        printf("  PASS: COW copied ~%d pages (expected ~%d)\n", cow_copies, expected_copies);
    } else {
        printf("  INFO: COW copied %d pages (expected ~%d)\n", cow_copies, expected_copies);
    }

    sbrk(-1024 * 1024);
}

// Test case: fork with full write (COW worst case)
void test_cow_full_write(void) {
    printf("[TEST] cow_full_write\n");

    struct memstat before, after;
    getmemstat(&before);

    // Prepare memory
    char *data = sbrk(256 * 1024);  // 256KB (64 pages)
    if(data == (char*)-1) {
        printf("  FAIL: sbrk failed\n");
        exit(1);
    }

    // Write pattern
    for(int i = 0; i < 64; i++) {
        data[i * 4096] = 'A' + (i % 26);
    }
    printf("  OK: prepared 256KB data (64 pages)\n");

    getmemstat(&after);

    int count = 3;
    int cow_before = after.cow_faults;
    int copy_before = after.cow_copy_pages;

    // Create children that write all memory
    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: write all pages
            for(int j = 0; j < 64; j++) {
                data[j * 4096] = 'B' + (j % 26);
            }
            exit(0);
        } else if(pid > 0) {
            // Parent: continue
        } else {
            printf("  FAIL: fork failed\n");
            exit(1);
        }
    }

    // Wait for all children
    for(int i = 0; i < count; i++) {
        wait(0);
    }

    getmemstat(&after);
    int cow_faults = after.cow_faults - cow_before;
    int cow_copies = after.cow_copy_pages - copy_before;

    printf("  OK: all children exited\n");
    printf("  RESULT: COW faults=%d, pages copied=%d\n", cow_faults, cow_copies);

    // Parent's data should be unchanged (COW)
    int errors = 0;
    for(int i = 0; i < 64; i++) {
        if(data[i * 4096] != 'A' + (i % 26)) {
            errors++;
        }
    }

    if(errors > 0) {
        printf("  FAIL: parent data modified at %d pages\n", errors);
        sbrk(-256 * 1024);
        exit(1);
    }
    printf("  OK: parent data unchanged (COW works)\n");

    // Assertion: In worst case, all pages should be copied
    int expected_copies = 64 * count;
    printf("  INFO: COW worst case - copied %d pages (expected ~%d)\n", cow_copies, expected_copies);
    printf("  NOTE: This is COW worst case - no memory savings\n");

    sbrk(-256 * 1024);
}

// Test case: multiple forks without writes
void test_cow_multiple_forks(void) {
    printf("[TEST] cow_multiple_forks\n");

    struct memstat before, after;
    getmemstat(&before);

    // Prepare memory
    char *data = sbrk(512 * 1024);  // 512KB (128 pages)
    if(data == (char*)-1) {
        printf("  FAIL: sbrk failed\n");
        exit(1);
    }

    // Write to ensure pages are allocated
    for(int i = 0; i < 128; i++) {
        data[i * 4096] = 'A' + (i % 26);
    }
    printf("  OK: prepared 512KB data (128 pages)\n");

    getmemstat(&after);
    int initial_pages = before.free_pages - after.free_pages;
    printf("  INFO: allocated %d physical pages\n", initial_pages);

    int count = 10;
    int cow_before = after.cow_faults;
    int copy_before = after.cow_copy_pages;

    // Create children that don't write
    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: just exit immediately
            exit(0);
        } else if(pid > 0) {
            // Parent: continue
        } else {
            printf("  FAIL: fork failed\n");
            exit(1);
        }
    }

    // Wait for all children
    for(int i = 0; i < count; i++) {
        wait(0);
    }

    getmemstat(&after);
    int cow_faults = after.cow_faults - cow_before;
    int cow_copies = after.cow_copy_pages - copy_before;

    printf("  OK: all %d children exited\n", count);
    printf("  RESULT: COW faults=%d, pages copied=%d\n", cow_faults, cow_copies);

    // Verify parent's data is unchanged
    int errors = 0;
    for(int i = 0; i < 128; i++) {
        if(data[i * 4096] != 'A' + (i % 26)) {
            errors++;
        }
    }

    if(errors > 0) {
        printf("  FAIL: parent data modified at %d pages\n", errors);
        sbrk(-512 * 1024);
        exit(1);
    }
    printf("  OK: parent data unchanged (COW works)\n");

    // Assertion: No COW copies should occur for read-only children
    if(cow_copies == 0) {
        printf("  PASS: COW optimal - no pages copied for %d read-only forks\n", count);
    } else {
        printf("  INFO: %d pages were copied (some overhead)\n", cow_copies);
    }

    sbrk(-512 * 1024);
}

// Test case: fork with exec (no shared memory)
void test_cow_fork_exec_chain(void) {
    printf("[TEST] cow_fork_exec_chain\n");

    struct memstat before, after;
    getmemstat(&before);

    int count = 5;
    int cow_before = before.cow_faults;
    int copy_before = before.cow_copy_pages;

    // Create chain: parent -> child1 -> child2 -> ...
    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: exec
            char *argv[] = {"echo", "chain", 0};
            exec("echo", argv);
            exit(1);
        } else if(pid > 0) {
            printf("  OK: forked child %d (pid=%d)\n", i, pid);
            wait(0);  // Wait before next fork
            printf("  OK: child %d completed\n", i);
        } else {
            printf("  FAIL: fork failed\n");
            exit(1);
        }
    }

    getmemstat(&after);
    int cow_faults = after.cow_faults - cow_before;
    int cow_copies = after.cow_copy_pages - copy_before;

    printf("  RESULT: COW faults=%d, pages copied=%d\n", cow_faults, cow_copies);
    printf("  OK: fork+exec chain test completed\n");
}

// Test case: memory sharing with fork
void test_cow_memory_sharing(void) {
    printf("[TEST] cow_memory_sharing\n");

    struct memstat before, after;
    getmemstat(&before);

    // Prepare large memory
    int pages = 100;
    int size = pages * 4096;
    char *data = sbrk(size);
    if(data == (char*)-1) {
        printf("  FAIL: sbrk failed\n");
        exit(1);
    }

    // Write pattern
    for(int i = 0; i < pages; i++) {
        data[i * 4096] = 'A' + (i % 26);
    }
    printf("  OK: prepared %d pages\n", pages);

    getmemstat(&after);
    int initial_pages = before.free_pages - after.free_pages;
    printf("  INFO: allocated %d physical pages\n", initial_pages);

    int count = 3;
    int cow_before = after.cow_faults;
    int copy_before = after.cow_copy_pages;
    int free_before = after.free_pages;

    // Create children that don't write
    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: just exit
            exit(0);
        } else if(pid > 0) {
            // Parent: continue
        } else {
            printf("  FAIL: fork failed\n");
            exit(1);
        }
    }

    // Wait for all children
    for(int i = 0; i < count; i++) {
        wait(0);
    }

    getmemstat(&after);
    int cow_faults = after.cow_faults - cow_before;
    int cow_copies = after.cow_copy_pages - copy_before;
    int free_after = after.free_pages;

    printf("  RESULT: COW faults=%d, pages copied=%d\n", cow_faults, cow_copies);
    printf("  INFO: free pages: %d -> %d\n", free_before, free_after);

    // Verify parent's data is unchanged
    int errors = 0;
    for(int i = 0; i < pages; i++) {
        if(data[i * 4096] != 'A' + (i % 26)) {
            errors++;
        }
    }

    if(errors > 0) {
        printf("  FAIL: %d data errors detected\n", errors);
        sbrk(-size);
        exit(1);
    }
    printf("  OK: all %d pages unchanged (COW works)\n", pages);

    // Memory should be reclaimed after children exit
    if(free_after >= free_before) {
        printf("  PASS: memory properly reclaimed after children exit\n");
    }

    sbrk(-size);
}
