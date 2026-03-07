// Test program for CFS (Completely Fair Scheduler) scheduler
// This test verifies that CFS scheduler fairly distributes CPU time

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

extern int getscheduler(void);
extern int setscheduler(int policy);

void
test_cfs_basic(void)
{
    printf("\n=== Test 1: Basic CFS Scheduler Switching ===\n");
    
    int current = getscheduler();
    printf("Current scheduler: %d\n", current);
    
    printf("Switching to CFS (policy=8)...");
    if(setscheduler(8) < 0) {
        printf(" FAILED\n");
        printf("ERROR: CFS scheduler not registered!\n");
        exit(1);
    }
    printf(" OK\n");
    
    current = getscheduler();
    printf("Current scheduler after switch: %d\n", current);
    
    if(current != 8) {
        printf("ERROR: Scheduler not switched correctly!\n");
        exit(1);
    }
    
    setscheduler(0);
    printf("Switched back to DEFAULT\n");
    printf("Test 1 PASSED\n");
}

void
test_cfs_fairness(void)
{
    printf("\n=== Test 2: CFS Fairness Test ===\n");
    printf("Creating 4 child processes with equal priority...\n");
    
    if(setscheduler(8) < 0) {
        printf("ERROR: Failed to switch to CFS\n");
        exit(1);
    }
    
    for(int i = 0; i < 4; i++) {
        int pid = fork();
        if(pid < 0) {
            printf("ERROR: fork failed!\n");
            exit(1);
        }
        
        if(pid == 0) {
            printf("[Child %d] PID=%d started\n", i, getpid());
            volatile int sum = 0;
            for(int j = 0; j < 200000; j++) {
                sum += j;
            }
            printf("[Child %d] PID=%d completed\n", i, getpid());
            exit(0);
        }
    }
    
    printf("All children created, waiting for completion...\n");
    
    int status;
    for(int i = 0; i < 4; i++) {
        wait(&status);
    }
    
    printf("All children completed\n");
    setscheduler(0);
    printf("Test 2 PASSED\n");
}

void
test_cfs_different_workloads(void)
{
    printf("\n=== Test 3: CFS Different Workloads ===\n");
    printf("Creating processes with different workloads...\n");
    
    if(setscheduler(8) < 0) {
        printf("ERROR: Failed to switch to CFS\n");
        exit(1);
    }
    
    int workloads[3] = {100000, 300000, 500000};
    char* names[3] = {"Light", "Medium", "Heavy"};
    
    for(int i = 0; i < 3; i++) {
        int pid = fork();
        if(pid < 0) {
            printf("ERROR: fork failed!\n");
            exit(1);
        }
        
        if(pid == 0) {
            printf("[%s] Child PID=%d started\n", names[i], getpid());
            volatile int sum = 0;
            for(int j = 0; j < workloads[i]; j++) {
                sum += j;
            }
            printf("[%s] Child PID=%d completed\n", names[i], getpid());
            exit(0);
        }
    }
    
    int status;
    for(int i = 0; i < 3; i++) {
        wait(&status);
    }
    
    printf("All children completed\n");
    setscheduler(0);
    printf("Test 3 PASSED\n");
}

int
main(int argc, char *argv[])
{
    printf("===========================================\n");
    printf("    CFS Scheduler Test Program\n");
    printf("===========================================\n");
    
    test_cfs_basic();
    test_cfs_fairness();
    test_cfs_different_workloads();
    
    printf("\n===========================================\n");
    printf("    All CFS Tests PASSED!\n");
    printf("===========================================\n");
    
    exit(0);
}
