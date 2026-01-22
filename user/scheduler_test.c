// Scheduler test program
// Tests different scheduling algorithms and collects performance statistics

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// Process information structure
struct proc_info {
    int pid;
    int priority;     // for PRIORITY
    int tickets;      // for LOTTERY
    int sched_class;  // for SML
    int rutime;      // running time
    int retime;      // ready time
    int stime;       // sleeping time
    char name[16];
};

// System call declarations (if not in user.h)
extern int getscheduler(void);
extern int setscheduler(int policy);

// Get process statistics (this would be a new syscall, for now we'll simulate)
// For now, we'll just test basic scheduler switching

void
test_scheduler_switching(void)
{
    printf("=== Testing Scheduler Switching ===\n");

    // Test getting current scheduler
    int current = getscheduler();
    printf("Current scheduler: %d\n", current);

    // Test switching to each scheduler
    const char* sched_names[] = {"DEFAULT", "FCFS", "PRIORITY", "SML", "LOTTERY"};

    for(int i = 0; i <= 4; i++) {
        printf("Switching to %s (policy=%d)...", sched_names[i], i);
        if(setscheduler(i) < 0) {
            printf(" FAILED\n");
        } else {
            printf(" OK\n");
        }
    }

    // Switch back to DEFAULT
    setscheduler(0);
    printf("Switched back to DEFAULT\n\n");
}

void
create_test_processes(void)
{
    printf("=== Creating Test Processes ===\n");

    int num_children = 5;

    for(int i = 0; i < num_children; i++) {
        int pid = fork();
        if(pid < 0) {
            printf("fork failed!\n");
            exit(1);
        }

        if(pid == 0) {
            // Child process
            printf("Child %d: PID=%d\n", i, getpid());

            // Do some work
            volatile int sum = 0;
            for(int j = 0; j < 1000000; j++) {
                sum += j;
            }

            exit(sum % 100);  // Exit with "result"
        }
    }

    // Parent waits for all children
    int status;
    for(int i = 0; i < num_children; i++) {
        wait(&status);
    }

    printf("All children completed\n\n");
}

void
test_fcfs(void)
{
    printf("=== Testing FCFS Scheduler ===\n");

    if(setscheduler(1) < 0) {
        printf("Failed to switch to FCFS\n");
        return;
    }

    int num_children = 3;
    for(int i = 0; i < num_children; i++) {
        int pid = fork();
        if(pid < 0) {
            printf("fork failed!\n");
            exit(1);
        }

        if(pid == 0) {
            // Child process - do work then exit
            volatile int sum = 0;
            for(int j = 0; j < 500000; j++) {
                sum += j;
            }
            (void)sum;  // Suppress unused warning
            exit(0);
        }
    }

    // Wait for all children
    int status;
    for(int i = 0; i < num_children; i++) {
        wait(&status);
    }

    printf("FCFS test completed\n");
    setscheduler(0);  // Switch back to DEFAULT
    printf("\n");
}

void
test_lottery(void)
{
    printf("=== Testing LOTTERY Scheduler ===\n");

    if(setscheduler(4) < 0) {
        printf("Failed to switch to LOTTERY\n");
        return;
    }

    // Create processes with different ticket counts
    // (Note: ticket system is already set in allocproc)

    int num_children = 4;
    for(int i = 0; i < num_children; i++) {
        int pid = fork();
        if(pid < 0) {
            printf("fork failed!\n");
            exit(1);
        }

        if(pid == 0) {
            // Child process
            // First child gets more tickets (simulated by doing more work)
            volatile int work = (i == 0) ? 2000000 : 500000;
            volatile int sum = 0;
            for(int j = 0; j < work; j++) {
                sum += j;
            }
            (void)sum;  // Suppress unused warning
            exit(0);
        }
    }

    // Wait for all children
    int status;
    for(int i = 0; i < num_children; i++) {
        wait(&status);
    }

    printf("LOTTERY test completed\n");
    setscheduler(0);  // Switch back to DEFAULT
    printf("\n");
}

int
main(int argc, char *argv[])
{
    printf("xv6-riscv Scheduler Test Program\n");
    printf("=====================================\n\n");

    // Test 1: Scheduler switching
    test_scheduler_switching();

    // Test 2: Create test processes with DEFAULT scheduler
    create_test_processes();

    // Test 3: FCFS scheduler
    test_fcfs();

    // Test 4: LOTTERY scheduler
    test_lottery();

    printf("=====================================\n");
    printf("All tests completed!\n");

    exit(0);
}
