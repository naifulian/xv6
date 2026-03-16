// Test program for system snapshot
// This test verifies that system snapshot correctly collects system state

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
test_basic_snapshot(void)
{
    printf("\n=== Test 1: Basic Snapshot ===\n");
    
    struct sys_snapshot snap;
    
    printf("Taking system snapshot...\n");
    if(getsnapshot(&snap) < 0) {
        printf("ERROR: getsnapshot failed!\n");
        exit(1);
    }
    
    printf("\n=== System Snapshot ===\n");
    printf("Timestamp: %d\n", (int)snap.timestamp);
    printf("CPU usage: %d%%\n", snap.cpu_usage);
    printf("Context switches: %d\n", (int)snap.context_switches);
    printf("Total ticks: %d\n", (int)snap.total_ticks);
    printf("Memory: %d/%d pages free\n", snap.free_pages, snap.total_pages);
    printf("Processes: %d total, %d running, %d sleeping, %d zombie\n",
           snap.nr_total, snap.nr_running, snap.nr_sleeping, snap.nr_zombie);
    printf("Scheduler: %d, Runqueue length: %d\n", snap.sched_policy, snap.runqueue_len);
    
    printf("Test 1 PASSED\n");
}

void
test_snapshot_after_work(void)
{
    printf("\n=== Test 2: Snapshot After Work ===\n");
    
    printf("Creating child processes...\n");
    
    for(int i = 0; i < 3; i++) {
        int pid = fork();
        if(pid < 0) {
            printf("ERROR: fork failed!\n");
            exit(1);
        }
        
        if(pid == 0) {
            volatile int sum = 0;
            for(int j = 0; j < 100000; j++) {
                sum += j;
            }
            exit(0);
        }
    }
    
    int status;
    for(int i = 0; i < 3; i++) {
        wait(&status);
    }
    
    struct sys_snapshot snap;
    
    printf("Taking snapshot after work...\n");
    if(getsnapshot(&snap) < 0) {
        printf("ERROR: getsnapshot failed!\n");
        exit(1);
    }
    
    printf("\n=== System Snapshot After Work ===\n");
    printf("Processes created/exited: check statistics\n");
    printf("Total processes: %d\n", snap.nr_total);
    printf("Context switches: %d\n", (int)snap.context_switches);
    
    printf("Test 2 PASSED\n");
}

void
test_snapshot_with_scheduler(void)
{
    printf("\n=== Test 3: Snapshot With Different Schedulers ===\n");
    
    struct sys_snapshot snap;
    
    for(int policy = 0; policy <= 8; policy++) {
        if(setscheduler(policy) < 0) {
            continue;
        }
        
        if(getsnapshot(&snap) < 0) {
            printf("ERROR: getsnapshot failed for policy %d!\n", policy);
            exit(1);
        }
        
        printf("Policy %d: CPU %d%%, Processes %d, Runqueue %d\n",
               snap.sched_policy, snap.cpu_usage, snap.nr_total, snap.runqueue_len);
    }
    
    setscheduler(0);
    printf("Test 3 PASSED\n");
}

int
main(int argc, char *argv[])
{
    printf("===========================================\n");
    printf("    System Snapshot Test Program\n");
    printf("===========================================\n");
    
    test_basic_snapshot();
    test_snapshot_after_work();
    test_snapshot_with_scheduler();
    
    printf("\n===========================================\n");
    printf("    All System Snapshot Tests PASSED!\n");
    printf("===========================================\n");
    
    exit(0);
}
