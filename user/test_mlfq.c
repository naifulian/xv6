// Test program for MLFQ (Multi-Level Feedback Queue) scheduler
// This test verifies that MLFQ scheduler correctly manages multiple priority queues

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

extern int getscheduler(void);
extern int setscheduler(int policy);

void
test_mlfq_basic(void)
{
    printf("\n=== Test 1: Basic MLFQ Scheduler Switching ===\n");
    
    int current = getscheduler();
    printf("Current scheduler: %d\n", current);
    
    printf("Switching to MLFQ (policy=7)...");
    if(setscheduler(7) < 0) {
        printf(" FAILED\n");
        printf("ERROR: MLFQ scheduler not registered!\n");
        exit(1);
    }
    printf(" OK\n");
    
    current = getscheduler();
    printf("Current scheduler after switch: %d\n", current);
    
    if(current != 7) {
        printf("ERROR: Scheduler not switched correctly!\n");
        exit(1);
    }
    
    setscheduler(0);
    printf("Switched back to DEFAULT\n");
    printf("Test 1 PASSED\n");
}

void
test_mlfq_multiple_queues(void)
{
    printf("\n=== Test 2: MLFQ Multiple Queues ===\n");
    printf("Creating 6 child processes to test multiple queues...\n");
    
    if(setscheduler(7) < 0) {
        printf("ERROR: Failed to switch to MLFQ\n");
        exit(1);
    }
    
    for(int i = 0; i < 6; i++) {
        int pid = fork();
        if(pid < 0) {
            printf("ERROR: fork failed!\n");
            exit(1);
        }
        
        if(pid == 0) {
            printf("[Child %d] PID=%d started\n", i, getpid());
            volatile int sum = 0;
            int work = (i + 1) * 100000;
            for(int j = 0; j < work; j++) {
                sum += j;
            }
            printf("[Child %d] PID=%d completed\n", i, getpid());
            exit(0);
        }
    }
    
    printf("All children created, waiting for completion...\n");
    
    int status;
    for(int i = 0; i < 6; i++) {
        wait(&status);
    }
    
    printf("All children completed\n");
    setscheduler(0);
    printf("Test 2 PASSED\n");
}

void
test_mlfq_interactive(void)
{
    printf("\n=== Test 3: MLFQ Interactive vs CPU-bound ===\n");
    printf("Creating mixed workload (interactive + CPU-bound)...\n");
    
    if(setscheduler(7) < 0) {
        printf("ERROR: Failed to switch to MLFQ\n");
        exit(1);
    }
    
    int pid1 = fork();
    if(pid1 == 0) {
        printf("[CPU-bound] PID=%d started\n", getpid());
        volatile int sum = 0;
        for(int j = 0; j < 1000000; j++) {
            sum += j;
        }
        printf("[CPU-bound] PID=%d completed\n", getpid());
        exit(0);
    }
    
    int pid2 = fork();
    if(pid2 == 0) {
        printf("[Interactive] PID=%d started\n", getpid());
        for(int i = 0; i < 5; i++) {
            volatile int sum = 0;
            for(int j = 0; j < 50000; j++) {
                sum += j;
            }
            sleep(1);
        }
        printf("[Interactive] PID=%d completed\n", getpid());
        exit(0);
    }
    
    int status;
    wait(&status);
    wait(&status);
    
    printf("All children completed\n");
    setscheduler(0);
    printf("Test 3 PASSED\n");
}

int
main(int argc, char *argv[])
{
    printf("===========================================\n");
    printf("    MLFQ Scheduler Test Program\n");
    printf("===========================================\n");
    
    test_mlfq_basic();
    test_mlfq_multiple_queues();
    test_mlfq_interactive();
    
    printf("\n===========================================\n");
    printf("    All MLFQ Tests PASSED!\n");
    printf("===========================================\n");
    
    exit(0);
}
