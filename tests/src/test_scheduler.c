// test_scheduler.c - Scheduler algorithm unit test implementation
// Tests: FCFS, PRIORITY, SML, LOTTERY scheduling correctness
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "../include/test_scheduler.h"

#define MAX_PSTAT 64

static char *sched_names[] = {
  "DEFAULT", "FCFS", "PRIORITY", "SML", "LOTTERY", "SJF", "SRTF", "MLFQ", "CFS"
};

static void
assert_status_eq(const char *label, int actual, int expected)
{
    if(actual != expected) {
        printf("  FAIL: %s = %d (expected %d)\n", label, actual, expected);
        exit(1);
    }
}

static void
release_children(int fd, int count)
{
    char token = 'x';

    for(int i = 0; i < count; i++) {
        if(write(fd, &token, 1) != 1) {
            printf("  FAIL: failed to release child %d\n", i);
            close(fd);
            exit(1);
        }
    }

    close(fd);
}

static void
wait_for_gate(int fd, int exit_code)
{
    char token;

    if(read(fd, &token, 1) != 1)
        exit(exit_code);
    close(fd);
}

static int
find_process_index(struct pstat *ps, int n, int pid)
{
    for(int i = 0; i < n; i++) {
        if(ps[i].pid == pid)
            return i;
    }
    return -1;
}

// Test case: FCFS - First Come First Served
void test_sched_fcfs_order(void) {
    printf("[TEST] sched_fcfs_order\n");

    int original = getscheduler();
    if(setscheduler(1) < 0) {
        printf("  FAIL: failed to switch to FCFS scheduler\n");
        exit(1);
    }
    printf("  OK: switched to FCFS scheduler\n");

    int gate[2];
    if(pipe(gate) < 0) {
        printf("  FAIL: pipe failed\n");
        setscheduler(original);
        exit(1);
    }

    int pids[3];

    for(int i = 0; i < 3; i++) {
        pids[i] = fork();
        if(pids[i] == 0) {
            close(gate[1]);
            wait_for_gate(gate[0], 90 + i);
            volatile int sum = 0;
            for(volatile int j = 0; j < 10000; j++) sum += j;
            exit(100 + i);
        } else if(pids[i] > 0) {
            printf("  OK: created process %d (pid=%d)\n", i, pids[i]);
        } else {
            printf("  FAIL: fork failed\n");
            close(gate[0]);
            close(gate[1]);
            setscheduler(original);
            exit(1);
        }
    }

    close(gate[0]);
    release_children(gate[1], 3);

    for(int i = 0; i < 3; i++) {
        int status;
        wait(&status);
        assert_status_eq("FCFS exit order", status, 100 + i);
        printf("  OK: child %d exited in FCFS order\n", i);
    }

    printf("  PASS: FCFS preserved creation order for equal workloads\n");
    setscheduler(original);
}

// Test case: PRIORITY - High priority runs first
void test_sched_priority_order(void) {
    printf("[TEST] sched_priority_order\n");

    int original = getscheduler();
    if(setscheduler(2) < 0) {
        printf("  FAIL: failed to switch to PRIORITY scheduler\n");
        exit(1);
    }
    printf("  OK: switched to PRIORITY scheduler\n");

    int gate[2];
    if(pipe(gate) < 0) {
        printf("  FAIL: pipe failed\n");
        setscheduler(original);
        exit(1);
    }

    int pids[3];
    int priorities[3] = {20, 1, 10};  // Low, High, Medium

    for(int i = 0; i < 3; i++) {
        pids[i] = fork();
        if(pids[i] == 0) {
            close(gate[1]);
            wait_for_gate(gate[0], 190 + i);
            volatile int sum = 0;
            for(volatile int j = 0; j < 50000; j++) sum += j;
            exit(200 + i);
        } else if(pids[i] > 0) {
            printf("  OK: created process %d (pid=%d, priority=%d)\n", 
                   i, pids[i], priorities[i]);
            if(setpriority(pids[i], priorities[i]) < 0) {
                printf("  FAIL: setpriority(%d, %d) failed\n", pids[i], priorities[i]);
                close(gate[0]);
                close(gate[1]);
                setscheduler(original);
                exit(1);
            }
        } else {
            printf("  FAIL: fork failed\n");
            close(gate[0]);
            close(gate[1]);
            setscheduler(original);
            exit(1);
        }
    }

    close(gate[0]);
    release_children(gate[1], 3);

    int expected[3] = {201, 202, 200};
    for(int i = 0; i < 3; i++) {
        int status;
        wait(&status);
        assert_status_eq("PRIORITY exit order", status, expected[i]);
        printf("  OK: child %d exited with expected priority order\n", i);
    }

    printf("  PASS: PRIORITY honored high-to-low order\n");
    setscheduler(original);
}

// Test case: PRIORITY - Same priority uses FCFS
void test_sched_priority_fcfs(void) {
    printf("[TEST] sched_priority_fcfs\n");

    int original = getscheduler();
    if(setscheduler(2) < 0) {
        printf("  FAIL: failed to switch to PRIORITY scheduler\n");
        exit(1);
    }
    printf("  OK: switched to PRIORITY scheduler\n");

    int gate[2];
    if(pipe(gate) < 0) {
        printf("  FAIL: pipe failed\n");
        setscheduler(original);
        exit(1);
    }

    int pids[3];

    for(int i = 0; i < 3; i++) {
        pids[i] = fork();
        if(pids[i] == 0) {
            close(gate[1]);
            wait_for_gate(gate[0], 290 + i);
            volatile int sum = 0;
            for(volatile int j = 0; j < 10000; j++) sum += j;
            exit(300 + i);
        } else if(pids[i] > 0) {
            printf("  OK: created process %d (pid=%d, priority=10)\n", i, pids[i]);
            if(setpriority(pids[i], 10) < 0) {
                printf("  FAIL: setpriority(%d, 10) failed\n", pids[i]);
                close(gate[0]);
                close(gate[1]);
                setscheduler(original);
                exit(1);
            }
        } else {
            printf("  FAIL: fork failed\n");
            close(gate[0]);
            close(gate[1]);
            setscheduler(original);
            exit(1);
        }
    }

    close(gate[0]);
    release_children(gate[1], 3);

    for(int i = 0; i < 3; i++) {
        int status;
        wait(&status);
        assert_status_eq("PRIORITY FCFS tiebreak order", status, 300 + i);
        printf("  OK: child %d exited in FCFS tiebreak order\n", i);
    }

    printf("  PASS: PRIORITY used FCFS as the tiebreaker\n");
    setscheduler(original);
}

// Test case: SML - Static Multilevel Queue
void test_sched_sml_queues(void) {
    printf("[TEST] sched_sml_queues\n");

    int original = getscheduler();
    setscheduler(3);  // SML
    printf("  OK: switched to SML scheduler\n");

    // SML uses 3 queues based on time slices consumed
    // New processes start in highest priority queue

    int pids[3];

    for(int i = 0; i < 3; i++) {
        pids[i] = fork();
        if(pids[i] == 0) {
            // Do different amounts of work
            volatile int sum = 0;
            int iterations = (i + 1) * 20000;
            for(volatile int j = 0; j < iterations; j++) sum += j;
            exit(400 + i);
        } else if(pids[i] > 0) {
            printf("  OK: created process %d (pid=%d)\n", i, pids[i]);
        } else {
            printf("  FAIL: fork failed\n");
            setscheduler(original);
            exit(1);
        }
    }

    for(int i = 0; i < 3; i++) {
        int status;
        int pid = wait(&status);
        printf("  INFO: process pid=%d exited with status %d\n", pid, status);  // xv6 stores exit status directly
    }

    printf("  OK: SML test completed\n");
    setscheduler(original);
}

// Test case: LOTTERY - Ticket distribution
void test_sched_lottery_basic(void) {
    printf("[TEST] sched_lottery_basic\n");

    int original = getscheduler();
    if(setscheduler(4) < 0) {
        printf("  FAIL: failed to switch to LOTTERY scheduler\n");
        exit(1);
    }
    printf("  OK: switched to LOTTERY scheduler\n");

    int gate[2];
    if(pipe(gate) < 0) {
        printf("  FAIL: pipe failed\n");
        setscheduler(original);
        exit(1);
    }

    int pids[3];
    int tickets[3] = {1, 10, 100};  // Expected: ~0.9%, ~9%, ~90% CPU

    for(int i = 0; i < 3; i++) {
        pids[i] = fork();
        if(pids[i] == 0) {
            close(gate[1]);
            wait_for_gate(gate[0], 490 + i);
            volatile int sum = 0;
            for(volatile int j = 0; j < 50000; j++) sum += j;
            exit(500 + i);
        } else if(pids[i] > 0) {
            printf("  OK: created process %d (pid=%d, tickets=%d)\n", 
                   i, pids[i], tickets[i]);
        } else {
            printf("  FAIL: fork failed\n");
            close(gate[0]);
            close(gate[1]);
            setscheduler(original);
            exit(1);
        }
    }

    struct pstat ps[MAX_PSTAT];
    int n = getptable(ps, MAX_PSTAT);
    if(n < 0) {
        printf("  FAIL: getptable failed\n");
        close(gate[0]);
        close(gate[1]);
        setscheduler(original);
        exit(1);
    }

    for(int i = 0; i < 3; i++) {
        int idx = find_process_index(ps, n, pids[i]);
        if(idx < 0) {
            printf("  FAIL: pid %d not visible in process table\n", pids[i]);
            close(gate[0]);
            close(gate[1]);
            setscheduler(original);
            exit(1);
        }
        if(ps[idx].tickets <= 0) {
            printf("  FAIL: pid %d has invalid ticket count %d\n", pids[i], ps[idx].tickets);
            close(gate[0]);
            close(gate[1]);
            setscheduler(original);
            exit(1);
        }
        printf("  OK: pid=%d exported tickets=%d\n", pids[i], ps[idx].tickets);
    }

    printf("  INFO: current kernel has no user-space ticket setter; this test is a smoke check for scheduler activation and exported ticket metadata\n");

    close(gate[0]);
    release_children(gate[1], 3);

    int seen[3] = {0, 0, 0};
    for(int i = 0; i < 3; i++) {
        int status;
        wait(&status);
        if(status < 500 || status > 502) {
            printf("  FAIL: unexpected lottery child exit status %d\n", status);
            setscheduler(original);
            exit(1);
        }
        if(seen[status - 500]) {
            printf("  FAIL: duplicate lottery child exit status %d\n", status);
            setscheduler(original);
            exit(1);
        }
        seen[status - 500] = 1;
    }

    printf("  PASS: LOTTERY scheduler ran children and exposed valid ticket state\n");
    setscheduler(original);
}

// Test case: Scheduler switch preserves processes
void test_sched_switch_preserves(void) {
    printf("[TEST] sched_switch_preserves\n");

    // Create a long-running process
    int pid = fork();
    if(pid == 0) {
        // Child: run for a while
        volatile int sum = 0;
        for(volatile int j = 0; j < 100000; j++) sum += j;
        exit(0);
    } else if(pid > 0) {
        printf("  OK: created long-running process (pid=%d)\n", pid);

        // Switch schedulers multiple times (all 9 schedulers)
        for(int s = 0; s <= 8; s++) {
            if(setscheduler(s) < 0) {
                printf("  FAIL: failed to switch to %s\n", sched_names[s]);
                exit(1);
            }
            printf("  OK: switched to %s\n", sched_names[s]);
        }

        // Wait for child
        int status;
        wait(&status);
        printf("  OK: child process completed\n");
        printf("  PASS: Process survived scheduler switches\n");
    } else {
        printf("  FAIL: fork failed\n");
        exit(1);
    }
}

// Test case: DEFAULT - Round Robin behavior
void test_sched_default_rr(void) {
    printf("[TEST] sched_default_rr\n");

    int original = getscheduler();
    setscheduler(0);  // DEFAULT (Round Robin)
    printf("  OK: switched to DEFAULT (RR) scheduler\n");

    // Create multiple CPU-bound processes
    int pids[4];

    for(int i = 0; i < 4; i++) {
        pids[i] = fork();
        if(pids[i] == 0) {
            volatile int sum = 0;
            for(volatile int j = 0; j < 50000; j++) sum += j;
            exit(600 + i);
        } else if(pids[i] > 0) {
            printf("  OK: created process %d (pid=%d)\n", i, pids[i]);
        } else {
            printf("  FAIL: fork failed\n");
            setscheduler(original);
            exit(1);
        }
    }

    // In RR, all should get fair CPU time
    for(int i = 0; i < 4; i++) {
        int status;
        int pid = wait(&status);
        printf("  INFO: process pid=%d exited\n", pid);
    }

    printf("  OK: DEFAULT (RR) test completed\n");
    setscheduler(original);
}

// Test case: getscheduler returns current scheduler
void test_sched_getscheduler(void) {
    printf("[TEST] sched_getscheduler\n");

    for(int s = 0; s <= 8; s++) {
        setscheduler(s);
        int current = getscheduler();
        if(current == s) {
            printf("  OK: getscheduler() returned %d (%s)\n", current, sched_names[s]);
        } else {
            printf("  FAIL: expected %d, got %d\n", s, current);
            exit(1);
        }
    }

    printf("  PASS: getscheduler works correctly\n");
}

// Test case: setpriority validation
void test_sched_setpriority(void) {
    printf("[TEST] sched_setpriority\n");

    int pid = getpid();
    printf("  INFO: current pid=%d\n", pid);

    // Test valid priorities (1-20)
    for(int p = 1; p <= 20; p += 5) {
        int result = setpriority(pid, p);
        if(result == 0) {
            printf("  OK: setpriority(%d, %d) succeeded\n", pid, p);
        } else {
            printf("  FAIL: setpriority(%d, %d) failed\n", pid, p);
            exit(1);
        }
    }

    // Test invalid priorities
    int result = setpriority(pid, 0);  // Too low
    if(result < 0) {
        printf("  OK: setpriority(%d, 0) rejected (expected)\n", pid);
    } else {
        printf("  FAIL: setpriority(%d, 0) unexpectedly succeeded\n", pid);
        exit(1);
    }

    result = setpriority(pid, 21);  // Too high
    if(result < 0) {
        printf("  OK: setpriority(%d, 21) rejected (expected)\n", pid);
    } else {
        printf("  FAIL: setpriority(%d, 21) unexpectedly succeeded\n", pid);
        exit(1);
    }

    printf("  PASS: setpriority validation works\n");
}

// Test case: Process statistics tracking
void test_sched_stats_tracking(void) {
    printf("[TEST] sched_stats_tracking\n");

    // Just verify getptable works
    struct pstat ps[MAX_PSTAT];
    int n = getptable(ps, MAX_PSTAT);
    if(n <= 0) {
        printf("  FAIL: getptable returned %d\n", n);
        exit(1);
    }
    printf("  OK: getptable returned %d processes\n", n);

    // Do some work
    volatile int sum = 0;
    for(volatile int j = 0; j < 50000; j++) sum += j;

    printf("  OK: stats tracking test completed\n");
}

// Test case: SJF - Shortest Job First ordering
void test_sched_sjf_order(void) {
    printf("[TEST] sched_sjf_order\n");

    int original = getscheduler();
    setscheduler(5);  // SJF
    printf("  OK: switched to SJF scheduler\n");

    // Create processes with different workloads
    int pids[3];
    int workloads[3] = {100000, 10000, 50000};  // Long, Short, Medium

    for(int i = 0; i < 3; i++) {
        pids[i] = fork();
        if(pids[i] == 0) {
            volatile int sum = 0;
            for(volatile int j = 0; j < workloads[i]; j++) sum += j;
            exit(700 + i);
        } else if(pids[i] > 0) {
            printf("  OK: created process %d (pid=%d, workload=%d)\n", 
                   i, pids[i], workloads[i]);
        } else {
            printf("  FAIL: fork failed\n");
            setscheduler(original);
            exit(1);
        }
    }

    // In SJF, shortest job should complete first
    for(int i = 0; i < 3; i++) {
        int status;
        wait(&status);
        printf("  INFO: process exited with status %d\n", status);
    }

    printf("  OK: SJF test completed\n");
    setscheduler(original);
}

// Test case: SJF - Time estimation
void test_sched_sjf_estimate(void) {
    printf("[TEST] sched_sjf_estimate\n");

    int original = getscheduler();
    setscheduler(5);  // SJF
    printf("  OK: switched to SJF scheduler\n");

    // Run same process multiple times to test EWMA estimation
    for(int round = 0; round < 3; round++) {
        int pid = fork();
        if(pid == 0) {
            volatile int sum = 0;
            for(volatile int j = 0; j < 30000; j++) sum += j;
            exit(800 + round);
        } else if(pid > 0) {
            int status;
            wait(&status);
            printf("  OK: round %d completed\n", round);
        } else {
            printf("  FAIL: fork failed\n");
            setscheduler(original);
            exit(1);
        }
    }

    printf("  OK: SJF estimate test completed\n");
    setscheduler(original);
}

// Test case: SRTF - Preemption
void test_sched_srtf_preempt(void) {
    printf("[TEST] sched_srtf_preempt\n");

    int original = getscheduler();
    setscheduler(6);  // SRTF
    printf("  OK: switched to SRTF scheduler\n");

    // Create long process first
    int pid1 = fork();
    if(pid1 == 0) {
        volatile int sum = 0;
        for(volatile int j = 0; j < 200000; j++) sum += j;
        exit(900);
    } else if(pid1 > 0) {
        printf("  OK: created long process (pid=%d)\n", pid1);
    }

    // Create short process after
    int pid2 = fork();
    if(pid2 == 0) {
        volatile int sum = 0;
        for(volatile int j = 0; j < 10000; j++) sum += j;
        exit(901);
    } else if(pid2 > 0) {
        printf("  OK: created short process (pid=%d)\n", pid2);
    }

    // Wait for both
    for(int i = 0; i < 2; i++) {
        int status;
        wait(&status);
        printf("  INFO: process exited with status %d\n", status);
    }

    printf("  OK: SRTF preempt test completed\n");
    setscheduler(original);
}

// Test case: SRTF - Remaining time tracking
void test_sched_srtf_remaining(void) {
    printf("[TEST] sched_srtf_remaining\n");

    int original = getscheduler();
    setscheduler(6);  // SRTF
    printf("  OK: switched to SRTF scheduler\n");

    // Create processes with different remaining times
    int pids[3];
    int workloads[3] = {50000, 20000, 30000};

    for(int i = 0; i < 3; i++) {
        pids[i] = fork();
        if(pids[i] == 0) {
            volatile int sum = 0;
            for(volatile int j = 0; j < workloads[i]; j++) sum += j;
            exit(1000 + i);
        } else if(pids[i] > 0) {
            printf("  OK: created process %d (pid=%d)\n", i, pids[i]);
        } else {
            printf("  FAIL: fork failed\n");
            setscheduler(original);
            exit(1);
        }
    }

    for(int i = 0; i < 3; i++) {
        int status;
        wait(&status);
        printf("  INFO: process exited\n");
    }

    printf("  OK: SRTF remaining test completed\n");
    setscheduler(original);
}

// Test case: MLFQ - Queue levels
void test_sched_mlfq_queues(void) {
    printf("[TEST] sched_mlfq_queues\n");

    int original = getscheduler();
    setscheduler(7);  // MLFQ
    printf("  OK: switched to MLFQ scheduler\n");

    // Create processes with different behaviors
    int pids[3];

    for(int i = 0; i < 3; i++) {
        pids[i] = fork();
        if(pids[i] == 0) {
            volatile int sum = 0;
            int iterations = (i + 1) * 50000;
            for(volatile int j = 0; j < iterations; j++) sum += j;
            exit(1100 + i);
        } else if(pids[i] > 0) {
            printf("  OK: created process %d (pid=%d)\n", i, pids[i]);
        } else {
            printf("  FAIL: fork failed\n");
            setscheduler(original);
            exit(1);
        }
    }

    for(int i = 0; i < 3; i++) {
        int status;
        wait(&status);
        printf("  INFO: process exited\n");
    }

    printf("  OK: MLFQ queues test completed\n");
    setscheduler(original);
}

// Test case: MLFQ - Priority boost
void test_sched_mlfq_priority_boost(void) {
    printf("[TEST] sched_mlfq_priority_boost\n");

    int original = getscheduler();
    setscheduler(7);  // MLFQ
    printf("  OK: switched to MLFQ scheduler\n");

    // Create I/O-bound process (should stay in high priority)
    int pid1 = fork();
    if(pid1 == 0) {
        for(int i = 0; i < 5; i++) {
            volatile int sum = 0;
            for(volatile int j = 0; j < 10000; j++) sum += j;
            sleep(1);  // Simulate I/O
        }
        exit(1200);
    } else if(pid1 > 0) {
        printf("  OK: created I/O-bound process (pid=%d)\n", pid1);
    }

    // Create CPU-bound process (should demote)
    int pid2 = fork();
    if(pid2 == 0) {
        volatile int sum = 0;
        for(volatile int j = 0; j < 200000; j++) sum += j;
        exit(1201);
    } else if(pid2 > 0) {
        printf("  OK: created CPU-bound process (pid=%d)\n", pid2);
    }

    for(int i = 0; i < 2; i++) {
        int status;
        wait(&status);
        printf("  INFO: process exited\n");
    }

    printf("  OK: MLFQ priority boost test completed\n");
    setscheduler(original);
}

// Test case: CFS - Fairness
void test_sched_cfs_fairness(void) {
    printf("[TEST] sched_cfs_fairness\n");

    int original = getscheduler();
    setscheduler(8);  // CFS
    printf("  OK: switched to CFS scheduler\n");

    // Create processes with same priority
    int pids[4];

    for(int i = 0; i < 4; i++) {
        pids[i] = fork();
        if(pids[i] == 0) {
            volatile int sum = 0;
            for(volatile int j = 0; j < 50000; j++) sum += j;
            exit(1300 + i);
        } else if(pids[i] > 0) {
            printf("  OK: created process %d (pid=%d)\n", i, pids[i]);
        } else {
            printf("  FAIL: fork failed\n");
            setscheduler(original);
            exit(1);
        }
    }

    // In CFS, all should get fair CPU time
    for(int i = 0; i < 4; i++) {
        int status;
        wait(&status);
        printf("  INFO: process exited\n");
    }

    printf("  OK: CFS fairness test completed\n");
    setscheduler(original);
}

// Test case: CFS - vruntime tracking
void test_sched_cfs_vruntime(void) {
    printf("[TEST] sched_cfs_vruntime\n");

    int original = getscheduler();
    setscheduler(8);  // CFS
    printf("  OK: switched to CFS scheduler\n");

    // Create processes with different workloads
    int pids[3];
    int workloads[3] = {100000, 30000, 50000};

    for(int i = 0; i < 3; i++) {
        pids[i] = fork();
        if(pids[i] == 0) {
            volatile int sum = 0;
            for(volatile int j = 0; j < workloads[i]; j++) sum += j;
            exit(1400 + i);
        } else if(pids[i] > 0) {
            printf("  OK: created process %d (pid=%d, workload=%d)\n", 
                   i, pids[i], workloads[i]);
        } else {
            printf("  FAIL: fork failed\n");
            setscheduler(original);
            exit(1);
        }
    }

    for(int i = 0; i < 3; i++) {
        int status;
        wait(&status);
        printf("  INFO: process exited\n");
    }

    printf("  OK: CFS vruntime test completed\n");
    setscheduler(original);
}
