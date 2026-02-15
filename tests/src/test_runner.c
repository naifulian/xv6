// test_runner.c - TDD test framework
#include "../include/test_common.h"
#include "../include/test_buddy.h"
#include "../include/test_mmap.h"
#include "../include/test_cow.h"
#include "../include/test_lazy.h"
#include "../include/test_integration.h"
#include "../include/test_boundary.h"
#include "../include/test_scheduler.h"

// List of all tests
struct test_case tests[] = {
    // Buddy system tests
    {"buddy_alloc_single", "Buddy alloc single page", test_buddy_alloc_single_page},
    {"buddy_alloc_multiple", "Buddy alloc multiple same", test_buddy_alloc_multiple_same},
    {"buddy_alloc_sizes", "Buddy alloc different sizes", test_buddy_alloc_different_sizes},
    {"buddy_data_integrity", "Buddy data integrity", test_buddy_data_integrity},
    {"buddy_merge", "Buddy merge pattern", test_buddy_merge_pattern},
    {"buddy_fragmentation", "Buddy fragmentation", test_buddy_fragmentation},
    {"buddy_stress", "Buddy stress small", test_buddy_stress_small},
    {"buddy_zero_size", "Buddy zero size", test_buddy_zero_size},
    {"buddy_large", "Buddy large allocation", test_buddy_large_allocation},
    {"buddy_free_null", "Buddy free NULL", test_buddy_free_null},
    {"buddy_double_free", "Buddy double free", test_buddy_double_free},

    // mmap tests (anonymous mapping only)
    {"mmap_basic", "mmap basic (anonymous)", test_mmap_basic},
    {"mmap_readonly", "mmap read-only (anonymous)", test_mmap_readonly},
    {"mmap_large", "mmap large (anonymous)", test_mmap_large},
    {"mmap_offset", "mmap offset (anonymous)", test_mmap_offset},
    {"mmap_multiple", "mmap multiple (anonymous)", test_mmap_multiple},
    {"mmap_munmap_null", "mmap munmap NULL", test_mmap_munmap_null},

    // COW tests
    {"cow_fork_exec", "COW fork+exec", test_cow_fork_exec},
    {"cow_partial_write", "COW partial write", test_cow_partial_write},
    {"cow_full_write", "COW full write", test_cow_full_write},
    {"cow_multiple", "COW multiple forks", test_cow_multiple_forks},
    {"cow_fork_exec_chain", "COW fork+exec chain", test_cow_fork_exec_chain},
    {"cow_memory_sharing", "COW memory sharing", test_cow_memory_sharing},

    // Lazy tests
    {"lazy_large_small", "Lazy large alloc small access", test_lazy_large_alloc_small_access},
    {"lazy_full_access", "Lazy large full access", test_lazy_large_alloc_full_access},
    {"lazy_sequential", "Lazy sequential access", test_lazy_sequential_access},
    {"lazy_random", "Lazy random access", test_lazy_random_access},
    {"lazy_multiple", "Lazy multiple allocs", test_lazy_multiple_allocs},
    {"lazy_zero", "Lazy zero alloc", test_lazy_zero_alloc},
    {"lazy_negative", "Lazy negative alloc", test_lazy_negative_alloc},
    {"lazy_very_large", "Lazy very large alloc", test_lazy_very_large_alloc},
    {"lazy_grow_sh_shrink", "Lazy grow and shrink", test_lazy_grow_sh_shrink},
    {"lazy_sparse", "Lazy sparse array", test_lazy_sparse_array},

    // Integration tests
    {"fork_exec_single", "fork+exec single", test_fork_exec_single},
    {"fork_exec_multiple", "fork+exec multiple", test_fork_exec_multiple},
    {"fork_exec_data", "fork+exec with data", test_fork_exec_with_data},
    {"fork_exec_chain", "fork+exec chain", test_fork_exec_chain},
    {"fork_exec_exit", "fork+exec with exit", test_fork_exec_exit_status},

    {"mmap_fork_private", "fork with data (simple)", test_mmap_fork_private},
    {"mmap_fork_shared", "multiple forks (no exec)", test_mmap_fork_shared},
    {"mmap_fork_multiple", "fork then exec", test_mmap_fork_multiple},
    {"mmap_fork_exec", "nested fork+exec", test_mmap_fork_exec},
    {"mmap_fork_anonymous", "mmap+fork anonymous", test_mmap_fork_anonymous},

    // Boundary tests
    {"nproc_limit", "NPROC limit", test_nproc_limit},
    {"nproc_exact", "NPROC exact", test_nproc_exact},
    {"nproc_fork_bomb", "NPROC fork bomb", test_nproc_fork_bomb},
    {"nproc_wait_refork", "NPROC wait refork", test_nproc_wait_refork},
    {"nproc_getpid", "NPROC getpid", test_nproc_getpid},
    {"nproc_orphaned", "NPROC orphaned", test_nproc_orphaned},

    // Scheduler tests
    {"sched_fcfs_order", "FCFS order", test_sched_fcfs_order},
    {"sched_priority_order", "PRIORITY order", test_sched_priority_order},
    {"sched_priority_fcfs", "PRIORITY FCFS tiebreak", test_sched_priority_fcfs},
    {"sched_sml_queues", "SML queues", test_sched_sml_queues},
    {"sched_lottery_basic", "LOTTERY basic", test_sched_lottery_basic},
    {"sched_switch_preserves", "Scheduler switch preserves", test_sched_switch_preserves},
    {"sched_default_rr", "DEFAULT RR", test_sched_default_rr},
    {"sched_getscheduler", "getscheduler", test_sched_getscheduler},
    {"sched_setpriority", "setpriority", test_sched_setpriority},
    {"sched_stats_tracking", "Stats tracking", test_sched_stats_tracking},
};

#define NUM_TESTS 60  // Total: 11+6+6+10+10+6+10 = 60 (buddy+mmap+cow+lazy+integ+bound+sched)
int num_tests = NUM_TESTS;

// Run a single test
int run_test(int index) {
    if(index < 0 || index >= num_tests) {
        printf("Invalid test index: %d\n", index);
        return 1;
    }

    printf("\n===== Test: %s =====\n", tests[index].name);
    printf("Description: %s\n\n", tests[index].description);

    // Run test and catch exit status
    int pid = fork();
    if(pid == 0) {
        // Child process
        tests[index].func();
        exit(0);
    } else if(pid > 0) {
        // Parent: wait for child
        int status;
        wait(&status);

        if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("Result: PASSED\n");
            return 0;
        } else if(WIFSIGNALED(status)) {
            printf("Result: FAILED (killed by signal %d)\n", WTERMSIG(status));
            return 1;
        } else {
            printf("Result: FAILED (exit status %d)\n", WEXITSTATUS(status));
            return 1;
        }
    } else {
        printf("Error: fork failed\n");
        return 1;
    }
}

// Run all tests
int run_all_tests(void) {
    int passed = 0;
    int failed = 0;

    printf("=== Running All TDD Tests ===\n\n");

    for(int i = 0; i < num_tests; i++) {
        int result = run_test(i);
        if(result == 0) {
            passed++;
        } else {
            failed++;
        }
    }

    printf("\n===== Test Summary =====\n");
    printf("Total:  %d\n", num_tests);
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);

    if(failed > 0) {
        return 1;
    }
    return 0;
}

// Print test list
void list_tests(void) {
    printf("=== Available Tests ===\n");
    for(int i = 0; i < num_tests; i++) {
        printf("%2d: %s - %s\n", i, tests[i].name, tests[i].description);
    }
    printf("Total: %d tests\n", num_tests);
}

// Main
int main(int argc, char *argv[]) {
    printf("=== TDD Test Runner for xv6 Tests ===\n\n");
    printf("Usage:\n");
    printf("  test_runner [all]           - Run all tests\n");
    printf("  test_runner [index]         - Run specific test by index\n");
    printf("  test_runner list             - List all tests\n\n\n");

    if(argc < 2) {
        printf("No arguments provided. Running all tests...\n\n");
        return run_all_tests();
    } else if(strcmp(argv[1], "all") == 0) {
        return run_all_tests();
    } else if(strcmp(argv[1], "list") == 0) {
        list_tests();
        return 0;
    } else {
        int index = atoi(argv[1]);
        return run_test(index);
    }
}
