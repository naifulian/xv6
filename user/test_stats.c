// Test program for CPU statistics
// This test verifies that CPU statistics are correctly tracked

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
test_basic_stats(void)
{
    printf("\n=== Test 1: Basic Statistics ===\n");
    
    printf("Getting CPU statistics...\n");
    if(getstats() < 0) {
        printf("ERROR: getstats failed!\n");
        exit(1);
    }
    
    printf("Test 1 PASSED\n");
}

void
test_stats_after_work(void)
{
    printf("\n=== Test 2: Statistics After Work ===\n");
    
    printf("Creating child processes to generate CPU activity...\n");
    
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
    
    printf("All children completed\n");
    printf("Getting CPU statistics after work...\n");
    
    if(getstats() < 0) {
        printf("ERROR: getstats failed!\n");
        exit(1);
    }
    
    printf("Test 2 PASSED\n");
}

void
test_stats_with_syscalls(void)
{
    printf("\n=== Test 3: Statistics With System Calls ===\n");
    
    printf("Making multiple system calls...\n");
    
    for(int i = 0; i < 10; i++) {
        getpid();
    }
    
    printf("Getting CPU statistics after syscalls...\n");
    
    if(getstats() < 0) {
        printf("ERROR: getstats failed!\n");
        exit(1);
    }
    
    printf("Test 3 PASSED\n");
}

int
main(int argc, char *argv[])
{
    printf("===========================================\n");
    printf("    CPU Statistics Test Program\n");
    printf("===========================================\n");
    
    test_basic_stats();
    test_stats_after_work();
    test_stats_with_syscalls();
    
    printf("\n===========================================\n");
    printf("    All CPU Statistics Tests PASSED!\n");
    printf("===========================================\n");
    
    exit(0);
}
