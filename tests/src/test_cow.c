// test_cow.c - Copy-on-Write unit test implementation
#include "../include/test_cow.h"

// Test case: fork without any write (COW optimal)
void test_cow_fork_exec(void) {
    printf("[TEST] cow_fork_exec\n");
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

    int count = 5;

    // Create children that immediately exec
    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: immediately exec
            char *argv[] = {"echo", "COW test", 0};
            exec("echo", argv);
            exit(1);  // Should not reach here
        } else if(pid > 0) {
            pid = pid;
            printf("  OK: forked child %d (pid=%d)\n", i, pid);
        } else {
            printf("  FAIL: fork failed\n");
            exit(1);
        }
    }

    // Wait for all children
    for(int i = 0; i < count; i++) {
        wait(0);
        printf("  OK: child %d exited\n", i);
    }

    printf("  OK: fork+exec test completed\n");
    sbrk(-1024 * 1024);  // Free memory
}

// Test case: fork with partial write (COW medium)
void test_cow_partial_write(void) {
    printf("[TEST] cow_partial_write\n");
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
    printf("  OK: prepared 1MB data\n");

    int count = 3;

    // Create children that write to part of memory
    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: write 10% of pages
            for(int j = 0; j < 26; j++) {
                data[j * 4096] = 'B' + (j % 26);
            }
            printf("  Child %d: wrote 26 pages\n", getpid());
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
        printf("  OK: child %d exited\n", i);
    }

    // Verify parent's data is unchanged (COW worked)
    for(int i = 0; i < 26; i++) {
        if(data[i * 4096] != 'A' + (i % 26)) {
            printf("  FAIL: parent data modified at page %d\n", i);
            sbrk(-1024 * 1024);
            exit(1);
        }
    }
    printf("  OK: parent data unchanged (COW works)\n");

    sbrk(-1024 * 1024);
}

// Test case: fork with full write (COW worst case)
void test_cow_full_write(void) {
    printf("[TEST] cow_full_write\n");
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

    int count = 3;

    // Create children that write all memory
    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: write all pages
            for(int j = 0; j < 64; j++) {
                data[j * 4096] = 'B' + (j % 26);
            }
            printf("  Child %d: wrote 64 pages\n", getpid());
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
        printf("  OK: child %d exited\n", i);
    }

    // Parent's data should be unchanged (COW)
    for(int i = 0; i < 64; i++) {
        if(data[i * 4096] != 'A' + (i % 26)) {
            printf("  FAIL: parent data modified at page %d\n", i);
            sbrk(-256 * 1024);
            exit(1);
        }
    }
    printf("  OK: parent data unchanged (COW works, but no benefit)\n");

    sbrk(-256 * 1024);
}

// Test case: multiple forks without writes
void test_cow_multiple_forks(void) {
    printf("[TEST] cow_multiple_forks\n");
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

    int count = 10;

    // Create children that don't write
    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: just exit immediately
            printf("  Child %d: exiting without write\n", getpid());
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
        printf("  OK: child %d exited\n", i);
    }

    // Verify parent's data is unchanged
    for(int i = 0; i < 128; i++) {
        if(data[i * 4096] != 'A' + (i % 26)) {
            printf("  FAIL: parent data modified at page %d\n", i);
            sbrk(-512 * 1024);
            exit(1);
        }
    }
    printf("  OK: parent data unchanged (COW works)\n");

    sbrk(-512 * 1024);
}

// Test case: fork with exec (no shared memory)
void test_cow_fork_exec_chain(void) {
    printf("[TEST] cow_fork_exec_chain\n");
    int count = 5;

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

    printf("  OK: fork+exec chain test completed\n");
}

// Test case: memory sharing with fork
void test_cow_memory_sharing(void) {
    printf("[TEST] cow_memory_sharing\n");
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

    int count = 3;

    // Create children that don't write
    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: just exit
            printf("  Child %d: exiting (COW should share memory)\n", getpid());
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

    sbrk(-size);
}
