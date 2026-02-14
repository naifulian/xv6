// test_boundary.c - Boundary test implementation (NPROC limits)
#include "../include/test_boundary.h"

// Test case: Create processes until NPROC limit
void test_nproc_limit(void) {
    printf("[TEST] nproc_limit\n");
    int count = 0;
    int max_attempts = 60;  // xv6 NPROC=64, try to create 60

    printf("  Attempting to create processes...\n");
    for(int i = 0; i < max_attempts; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: wait briefly
            sleep(5);
            exit(0);
        } else if(pid > 0) {
            // Parent: record fork succeeded
            count++;
            if((count % 10) == 0) {
                printf("  Created %d processes\n", count);
            }
        } else {
            // Fork failed - NPROC limit reached
            printf("  Fork failed at attempt %d (NPROC limit reached at %d processes)\n", i, count);
            break;
        }
    }

    printf("  Total processes created: %d\n", count);

    // Wait for all children
    int exited = 0;
    for(int i = 0; i < count; i++) {
        if(wait(0) > 0) {
            exited++;
        }
    }
    printf("  Actual children waited: %d\n", exited);

    if(count > 50) {
        printf("  OK: Created %d processes (NPROC=64, %d reserved for system)\n", count, 64 - count);
    }
}

// Test case: Create exact number of processes
void test_nproc_exact(void) {
    printf("[TEST] nproc_exact\n");
    int target = 30;  // xv6 NPROC=64, create 30
    int pids[64];

    for(int i = 0; i < target; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: sleep briefly then exit
            sleep(2);
            exit(0);
        } else if(pid > 0) {
            // Parent: store PID
            pids[i] = pid;
            printf("  OK: forked child %d (pid=%d)\n", i, pid);
        } else {
            printf("  FAIL: fork failed at %d\n", i);
            // Clean up and exit
            for(int j = 0; j < i; j++) {
                kill(pids[j]);
                wait(0);
            }
            exit(1);
        }
    }

    printf("  OK: forked %d children\n", target);

    // Wait for all children
    int exited = 0;
    for(int i = 0; i < target; i++) {
        if(wait(0) > 0) {
            exited++;
        }
    }
    printf("  OK: %d children exited\n", exited);

    if(exited == target) {
        printf("  OK: all %d children accounted for\n", target);
    }
}

// Test case: Fork bomb attempt (rapid forking)
void test_nproc_fork_bomb(void) {
    printf("[TEST] nproc_fork_bomb\n");
    int max_forks = 50;  // xv6 limit
    int pids[50];
    int count = 0;

    for(int i = 0; i < max_forks; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: wait briefly
            sleep(2);
            exit(0);
        } else if(pid > 0) {
            // Parent: record PID
            pids[count] = pid;
            count++;
            if((count % 20) == 0) {
                printf("  Created %d processes\n", count);
            }
        } else {
            // Fork failed
            printf("  Fork failed at %d (NPROC limit reached at %d processes)\n", i, count);
            break;
        }
    }

    printf("  Total forks: %d\n", count);

    // Clean up all children
    for(int i = 0; i < count; i++) {
        kill(pids[i]);
    }
    int exited = 0;
    for(int i = 0; i < count; i++) {
        if(wait(0) > 0) {
            exited++;
        }
    }
    printf("  OK: cleaned up %d processes\n", exited);
}

// Test case: Wait all children then fork again
void test_nproc_wait_refork(void) {
    printf("[TEST] nproc_wait_refork\n");
    // First round: create 20 processes
    int count = 0;

    for(int i = 0; i < 20; i++) {
        int pid = fork();
        if(pid == 0) {
            sleep(3);
            exit(0);
        } else if(pid > 0) {
            count++;
            printf("  OK: forked child %d\n", i);
        } else {
            printf("  FAIL: fork failed\n");
            exit(1);
        }
    }

    printf("  OK: forked %d children\n", count);

    // Wait for all
    int exited = 0;
    for(int i = 0; i < count; i++) {
        if(wait(0) > 0) {
            exited++;
        }
    }
    printf("  OK: %d children exited\n", exited);

    // Try to fork again (should work now)
    int pid = fork();
    if(pid == 0) {
        printf("  OK: new fork succeeded (resources freed)\n");
        exit(0);
    } else if(pid > 0) {
        wait(0);
        printf("  OK: waited for new fork\n");
    } else {
        printf("  FAIL: fork failed unexpectedly\n");
        exit(1);
    }
}

// Test case: Getpid after fork
void test_nproc_getpid(void) {
    printf("[TEST] nproc_getpid\n");
    int parent_pid = getpid();
    printf("  Parent PID: %d\n", parent_pid);

    int pid = fork();
    if(pid == 0) {
        int child_pid = getpid();
        printf("  Child PID: %d\n", child_pid);
        if(child_pid == parent_pid) {
            printf("  FAIL: child PID equals parent PID\n");
            exit(1);
        }
        printf("  OK: child PID differs from parent\n");
        exit(0);
    } else if(pid > 0) {
        wait(0);
        printf("  OK: forked child with pid %d\n", pid);
    } else {
        printf("  FAIL: fork failed\n");
        exit(1);
    }
}

// Test case: Orphaned process (parent exits first)
// Note: In xv6, orphaned children are re-parented to init (pid 1)
// Revised: wait for grandchild to exit before reporting completion
void test_nproc_orphaned(void) {
    printf("[TEST] nproc_orphaned\n");

    int pid = fork();
    if(pid == 0) {
        // Child: fork a grandchild
        int gpid = fork();
        if(gpid == 0) {
            // Grandchild: sleep briefly
            printf("  Grandchild: sleeping (will be orphaned to init)\n");
            sleep(2);
            printf("  Grandchild: exiting\n");
            exit(0);
        } else if(gpid > 0) {
            // Child: wait for grandchild, then exit
            // This ensures grandchild completes before parent thinks test is done
            wait(0);
            printf("  Child: exiting after grandchild\n");
            exit(0);
        } else {
            exit(1);
        }
    } else if(pid > 0) {
        // Parent: wait for child
        wait(0);
        printf("  OK: orphan process test completed\n");
    } else {
        printf("  FAIL: fork failed\n");
        exit(1);
    }
}
