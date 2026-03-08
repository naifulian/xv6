// Scheduler Demo - Demonstrates different scheduler behaviors
// Shows performance differences between scheduling algorithms

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

extern int getscheduler(void);
extern int setscheduler(int policy);

void
run_cpu_bound_workload(int processes)
{
    printf("Running %d CPU-bound processes...\n", processes);
    
    int start_time = uptime();
    
    for(int i = 0; i < processes; i++) {
        int pid = fork();
        if(pid < 0) {
            printf("ERROR: fork failed!\n");
            exit(1);
        }
        
        if(pid == 0) {
            volatile int sum = 0;
            for(int j = 0; j < 2000000; j++) {
                sum += j;
            }
            exit(0);
        }
    }
    
    int status;
    for(int i = 0; i < processes; i++) {
        wait(&status);
    }
    
    int end_time = uptime();
    int elapsed = end_time - start_time;
    
    printf("Completed in %d ticks\n", elapsed);
}

void
run_mixed_workload(int processes)
{
    printf("Running %d mixed workload processes...\n", processes);
    
    int start_time = uptime();
    
    for(int i = 0; i < processes; i++) {
        int pid = fork();
        if(pid < 0) {
            printf("ERROR: fork failed!\n");
            exit(1);
        }
        
        if(pid == 0) {
            if(i % 2 == 0) {
                volatile int sum = 0;
                for(int j = 0; j < 1000000; j++) {
                    sum += j;
                }
            } else {
                for(int k = 0; k < 5; k++) {
                    volatile int sum = 0;
                    for(int j = 0; j < 200000; j++) {
                        sum += j;
                    }
                    sleep(1);
                }
            }
            exit(0);
        }
    }
    
    int status;
    for(int i = 0; i < processes; i++) {
        wait(&status);
    }
    
    int end_time = uptime();
    int elapsed = end_time - start_time;
    
    printf("Completed in %d ticks\n", elapsed);
}

void
run_io_bound_workload(int processes)
{
    printf("Running %d I/O-bound processes...\n", processes);
    
    int start_time = uptime();
    
    for(int i = 0; i < processes; i++) {
        int pid = fork();
        if(pid < 0) {
            printf("ERROR: fork failed!\n");
            exit(1);
        }
        
        if(pid == 0) {
            for(int k = 0; k < 8; k++) {
                volatile int sum = 0;
                for(int j = 0; j < 100000; j++) {
                    sum += j;
                }
                sleep(1);
            }
            exit(0);
        }
    }
    
    int status;
    for(int i = 0; i < processes; i++) {
        wait(&status);
    }
    
    int end_time = uptime();
    int elapsed = end_time - start_time;
    
    printf("Completed in %d ticks\n", elapsed);
}

void
test_scheduler(int policy, const char *name)
{
    printf("\n=== Testing %s ===\n", name);
    
    if(setscheduler(policy) < 0) {
        printf("ERROR: Failed to set scheduler %d\n", policy);
        return;
    }
    
    printf("\n--- CPU-bound workload ---\n");
    run_cpu_bound_workload(4);
    
    printf("\n--- Mixed workload ---\n");
    run_mixed_workload(4);
    
    printf("\n--- I/O-bound workload ---\n");
    run_io_bound_workload(4);
}

int
main(int argc, char *argv[])
{
    printf("===========================================\n");
    printf("    Scheduler Demo Program\n");
    printf("===========================================\n");
    printf("\nThis program demonstrates different scheduler behaviors\n");
    printf("by running various workloads under each scheduler.\n\n");
    
    test_scheduler(0, "DEFAULT (Round-Robin)");
    test_scheduler(1, "FCFS");
    test_scheduler(2, "PRIORITY");
    test_scheduler(3, "SML");
    test_scheduler(4, "LOTTERY");
    test_scheduler(5, "SJF");
    test_scheduler(6, "SRTF");
    test_scheduler(7, "MLFQ");
    test_scheduler(8, "CFS");
    
    setscheduler(0);
    
    printf("\n===========================================\n");
    printf("    Demo Complete!\n");
    printf("===========================================\n");
    
    exit(0);
}
