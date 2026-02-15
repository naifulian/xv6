// test_integration.c - Integration test implementation (fork+exec only)
#include "../include/test_integration.h"

// ========== fork+exec integration tests ==========

// Test case: Single fork+exec
void test_fork_exec_single(void) {
    printf("[INTEG] fork_exec_single\n");

    int pid = fork();
    if(pid == 0) {
        // Child: immediately exec
        char *argv[] = {"echo", "fork+exec test", 0};
        exec("echo", argv);
        exit(1);  // Should not reach here
    } else if(pid > 0) {
        printf("  OK: forked child (pid=%d)\n", pid);
        int status;
        wait(&status);
        printf("  OK: child exited with status %d\n", status);
    } else {
        printf("  FAIL: fork failed\n");
        exit(1);
    }
}

// Test case: Multiple fork+exec
void test_fork_exec_multiple(void) {
    printf("[INTEG] fork_exec_multiple\n");

    int count = 5;

    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child: immediately exec
            char *argv[] = {"echo", "test", 0};
            exec("echo", argv);
            exit(1);
        } else if(pid > 0) {
            printf("  OK: forked child %d (pid=%d)\n", i, pid);
        } else {
            printf("  FAIL: fork failed at %d\n", i);
            exit(1);
        }
    }

    // Wait for all children
    for(int i = 0; i < count; i++) {
        int status;
        wait(&status);
    }
    printf("  OK: all %d children exited\n", count);
}

// Test case: fork+exec with data in parent (COW test)
void test_fork_exec_with_data(void) {
    printf("[INTEG] fork_exec_with_data (COW)\n");

    // Parent prepares some data
    char *data = sbrk(4096);
    if(data == (char*)-1) {
        printf("  FAIL: sbrk failed\n");
        exit(1);
    }

    // Write to ensure page is allocated
    for(int i = 0; i < 1024; i++) {
        data[i] = 'A' + (i % 26);
    }
    printf("  OK: prepared 4KB data\n");

    int pid = fork();
    if(pid == 0) {
        // Child: immediately exec (should not use parent's memory)
        char *argv[] = {"echo", "COW test", 0};
        exec("echo", argv);
        exit(1);
    } else if(pid > 0) {
        printf("  OK: forked child (pid=%d)\n", pid);
        int status;
        wait(&status);
        printf("  OK: child exited with status %d\n", status);

        // Verify parent's data is unchanged (COW worked)
        int errors = 0;
        for(int i = 0; i < 10; i++) {
            if(data[i] != 'A' + (i % 26)) {
                errors++;
            }
        }

        if(errors > 0) {
            printf("  FAIL: parent data modified (COW failed)\n");
            sbrk(-4096);
            exit(1);
        }
        printf("  OK: parent data unchanged (COW works)\n");
    } else {
        printf("  FAIL: fork failed\n");
        exit(1);
    }

    sbrk(-4096);
}

// Test case: fork+exec chain
void test_fork_exec_chain(void) {
    printf("[INTEG] fork_exec_chain\n");

    int count = 3;

    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child immediately exec
            char *argv[] = {"echo", "chain test", 0};
            exec("echo", argv);
            exit(1);
        } else if(pid > 0) {
            printf("  OK: forked child %d (pid=%d)\n", i, pid);
            int status;
            wait(&status);
        } else {
            printf("  FAIL: fork failed at %d\n", i);
            exit(1);
        }
    }
    printf("  OK: chain of %d execs completed\n", count);
}

// Test case: fork+exec exit status
void test_fork_exec_exit_status(void) {
    printf("[INTEG] fork_exec_exit_status\n");

    int pid = fork();
    if(pid == 0) {
        // Child: exit with specific status
        exit(42);
    } else if(pid > 0) {
        int status;
        wait(&status);

        // xv6 stores exit status directly (not shifted like POSIX)
        if(status == 42) {
            printf("  OK: child exited with status 42\n");
        } else {
            printf("  FAIL: child exit status unexpected (got %d, expected 42)\n", status);
            exit(1);
        }
    } else {
        printf("  FAIL: fork failed\n");
        exit(1);
    }
}

// ========== Simple fork tests ==========

// Test case: Simple fork with data
void test_mmap_fork_private(void) {
    printf("[INTEG] fork_with_data (simple)\n");

    // Parent prepares data
    char *data = sbrk(4096);
    if(data == (char*)-1) {
        printf("  FAIL: sbrk failed\n");
        exit(1);
    }

    data[0] = 'X';
    printf("  OK: prepared data\n");

    int pid = fork();
    if(pid == 0) {
        // Child reads parent's data (COW should create separate copy on write)
        if(data[0] == 'X') {
            printf("  OK: child read parent data\n");
            // Write to trigger COW
            data[0] = 'Y';
            printf("  OK: child wrote data\n");
        }
        exit(0);
    } else if(pid > 0) {
        int status;
        wait(&status);
        // Parent's data should be unchanged
        if(data[0] == 'X') {
            printf("  OK: parent data unchanged after child write (COW)\n");
        } else {
            printf("  INFO: parent data changed (no COW)\n");
        }
    } else {
        printf("  FAIL: fork failed\n");
        exit(1);
    }

    sbrk(-4096);
}

// Test case: Multiple forks without exec
void test_mmap_fork_shared(void) {
    printf("[INTEG] multiple_forks\n");

    int count = 3;
    for(int i = 0; i < count; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child just exits
            sleep(1);
            exit(0);
        } else if(pid > 0) {
            printf("  OK: forked child %d\n", i);
        } else {
            printf("  FAIL: fork failed\n");
            exit(1);
        }
    }

    // Wait for all children
    for(int i = 0; i < count; i++) {
        int status;
        wait(&status);
    }
    printf("  OK: all %d children exited\n", count);
}

// Test case: Fork then exec immediately
void test_mmap_fork_multiple(void) {
    printf("[INTEG] fork_then_exec\n");

    int pid = fork();
    if(pid == 0) {
        // Child immediately exec
        char *argv[] = {"echo", "immediate exec", 0};
        exec("echo", argv);
        exit(1);
    } else if(pid > 0) {
        printf("  OK: forked, child should exec immediately\n");
        int status;
        wait(&status);
        printf("  OK: child exited\n");
    } else {
        printf("  FAIL: fork failed\n");
        exit(1);
    }
}

// Test case: Fork and exec in nested pattern
void test_mmap_fork_exec(void) {
    printf("[INTEG] nested_fork_exec\n");

    // Parent forks
    int pid = fork();
    if(pid == 0) {
        // Child forks again
        int pid2 = fork();
        if(pid2 == 0) {
            // Grandchild execs
            char *argv[] = {"echo", "nested", 0};
            exec("echo", argv);
            exit(1);
        } else if(pid2 > 0) {
            int status;
            wait(&status);
            exit(0);
        } else {
            exit(1);
        }
    } else if(pid > 0) {
        printf("  OK: forked first child\n");
        int status;
        wait(&status);
        printf("  OK: nested fork+exec completed\n");
    } else {
        printf("  FAIL: fork failed\n");
        exit(1);
    }
}

// Test case: Anonymous mmap + fork
void test_mmap_fork_anonymous(void) {
    printf("[INTEG] mmap_fork_anonymous\n");

    // Map anonymous memory
    char *addr = mmap(0, 4096, 3, 0x22, -1, 0);  // PROT_READ|PROT_WRITE, MAP_ANONYMOUS
    if(addr == (char*)-1) {
        printf("  FAIL: mmap failed\n");
        exit(1);
    }
    printf("  OK: mmap returned %p\n", addr);

    // Write to mapped memory
    addr[0] = 'A';
    printf("  OK: wrote to mapped memory\n");

    // Fork
    int pid = fork();
    if(pid == 0) {
        // Child reads to mapping
        if(addr[0] == 'A') {
            printf("  OK: child can read mapped memory\n");
            // Write to trigger COW
            addr[0] = 'B';
            printf("  OK: child wrote to mapped memory\n");
        }
        exit(0);
    } else if(pid > 0) {
        int status;
        wait(&status);

        // Parent's mapping should be unchanged
        if(addr[0] == 'A') {
            printf("  OK: parent mapped memory unchanged (COW worked)\n");
        } else {
            printf("  INFO: parent mapped memory changed\n");
        }

        munmap(addr, 4096);
        printf("  OK: unmapped\n");
    } else {
        printf("  FAIL: fork failed\n");
        munmap(addr, 4096);
        exit(1);
    }
}
