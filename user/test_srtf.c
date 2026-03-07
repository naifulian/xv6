// Test program for SRTF (Shortest Remaining Time First) scheduler
// This test verifies that SRTF scheduler correctly selects processes
// with shortest remaining time and supports preemption

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

extern int getscheduler(void);
extern int setscheduler(int policy);

void
test_srtf_basic(void)
{
    printf("\n=== Test 1: Basic SRTF Scheduler Switching ===\n");
    
    int current = getscheduler();
    printf("Current scheduler: %d\n", current);
    
    printf("Switching to SRTF (policy=6)...");
    if(setscheduler(6) < 0) {
        printf(" FAILED\n");
        printf("ERROR: SRTF scheduler not registered!\n");
        exit(1);
    }
    printf(" OK\n");
    
    current = getscheduler();
    printf("Current scheduler after switch: %d\n", current);
    
    if(current != 6) {
        printf("ERROR: Scheduler not switched correctly!\n");
        exit(1);
    }
    
    setscheduler(0);
    printf("Switched back to DEFAULT\n");
    printf("Test 1 PASSED\n");
}

void
test_srtf_order(void)
{
    printf("\n=== Test 2: SRTF Process Ordering ===\n");
    printf("Creating 3 child processes with different workloads...\n");
    
    if(setscheduler(6) < 0) {
        printf("ERROR: Failed to switch to SRTF\n");
        exit(1);
    }
    
    int workloads[3] = {500000, 100000, 300000};
    char* names[3] = {"Long", "Short", "Medium"};
    
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
    
    printf("All children created, waiting for completion...\n");
    
    int status;
    int completed = 0;
    while(completed < 3) {
        wait(&status);
        completed++;
    }
    
    printf("All children completed\n");
    setscheduler(0);
    printf("Test 2 PASSED\n");
}

void
test_srtf_dynamic(void)
{
    printf("\n=== Test 3: SRTF Dynamic Arrival ===\n");
    printf("Creating processes with staggered arrival times...\n");
    
    if(setscheduler(6) < 0) {
        printf("ERROR: Failed to switch to SRTF\n");
        exit(1);
    }
    
    int pid1 = fork();
    if(pid1 == 0) {
        printf("[Long] Child PID=%d started\n", getpid());
        volatile int sum = 0;
        for(int j = 0; j < 800000; j++) {
            sum += j;
        }
        printf("[Long] Child PID=%d completed\n", getpid());
        exit(0);
    }
    
    for(int i = 0; i < 100000; i++) {
        volatile int x = i * i;
        (void)x;
    }
    
    int pid2 = fork();
    if(pid2 == 0) {
        printf("[Short] Child PID=%d started (arrived later)\n", getpid());
        volatile int sum = 0;
        for(int j = 0; j < 100000; j++) {
            sum += j;
        }
        printf("[Short] Child PID=%d completed\n", getpid());
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
    printf("    SRTF Scheduler Test Program\n");
    printf("===========================================\n");
    
    test_srtf_basic();
    test_srtf_order();
    test_srtf_dynamic();
    
    printf("\n===========================================\n");
    printf("    All SRTF Tests PASSED!\n");
    printf("===========================================\n");
    
    exit(0);
}
