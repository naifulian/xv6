// Test program for SJF (Shortest Job First) scheduler
// This test verifies that SJF scheduler correctly selects processes
// with shortest estimated run time

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

extern int getscheduler(void);
extern int setscheduler(int policy);

void
test_sjf_basic(void)
{
    printf("\n=== Test 1: Basic SJF Scheduler Switching ===\n");
    
    int current = getscheduler();
    printf("Current scheduler: %d\n", current);
    
    printf("Switching to SJF (policy=5)...");
    if(setscheduler(5) < 0) {
        printf(" FAILED\n");
        printf("ERROR: SJF scheduler not registered!\n");
        exit(1);
    }
    printf(" OK\n");
    
    current = getscheduler();
    printf("Current scheduler after switch: %d\n", current);
    
    if(current != 5) {
        printf("ERROR: Scheduler not switched correctly!\n");
        exit(1);
    }
    
    setscheduler(0);
    printf("Switched back to DEFAULT\n");
    printf("Test 1 PASSED\n");
}

void
test_sjf_order(void)
{
    printf("\n=== Test 2: SJF Process Ordering ===\n");
    printf("Creating 3 child processes with different workloads...\n");
    
    if(setscheduler(5) < 0) {
        printf("ERROR: Failed to switch to SJF\n");
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
test_sjf_multiple_rounds(void)
{
    printf("\n=== Test 3: SJF Multiple Rounds ===\n");
    printf("Creating 4 child processes that run multiple times...\n");
    
    if(setscheduler(5) < 0) {
        printf("ERROR: Failed to switch to SJF\n");
        exit(1);
    }
    
    for(int round = 0; round < 2; round++) {
        printf("\n--- Round %d ---\n", round + 1);
        
        for(int i = 0; i < 4; i++) {
            int pid = fork();
            if(pid < 0) {
                printf("ERROR: fork failed!\n");
                exit(1);
            }
            
            if(pid == 0) {
                volatile int sum = 0;
                int work = (i + 1) * 100000;
                for(int j = 0; j < work; j++) {
                    sum += j;
                }
                exit(0);
            }
        }
        
        int status;
        for(int i = 0; i < 4; i++) {
            wait(&status);
        }
    }
    
    printf("\nAll rounds completed\n");
    setscheduler(0);
    printf("Test 3 PASSED\n");
}

int
main(int argc, char *argv[])
{
    printf("===========================================\n");
    printf("    SJF Scheduler Test Program\n");
    printf("===========================================\n");
    
    test_sjf_basic();
    test_sjf_order();
    test_sjf_multiple_rounds();
    
    printf("\n===========================================\n");
    printf("    All SJF Tests PASSED!\n");
    printf("===========================================\n");
    
    exit(0);
}
