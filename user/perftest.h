/*
 * perftest.h - Performance Test Naming Convention
 * 
 * This header defines standardized test names for both baseline and testing branches.
 * Both branches MUST use the same naming to ensure data comparability.
 * 
 * Usage:
 *   1. Include this header in perftest.c
 *   2. Use TEST_* macros as the test name in print_result()
 *   3. plot_results.py will parse these names for comparison
 */

#ifndef _PERFTEST_H_
#define _PERFTEST_H_

/*
 * COW (Copy-on-Write) Tests
 * 
 * Test scenarios:
 *   - NO_ACCESS:  fork + child exits immediately (no memory access)
 *   - READONLY:   fork + child reads all pages
 *   - PARTIAL:    fork + child writes 30% of pages
 *   - FULLWRITE:  fork + child writes 100% of pages (counter-example)
 */
#define TEST_COW_NO_ACCESS    "COW_NO_ACCESS"
#define TEST_COW_READONLY     "COW_READONLY"
#define TEST_COW_PARTIAL      "COW_PARTIAL"
#define TEST_COW_FULLWRITE    "COW_FULLWRITE"

/*
 * Lazy Allocation Tests
 * 
 * Test scenarios:
 *   - SPARSE:  allocate 1MB, access 1% (best case)
 *   - HALF:    allocate 1MB, access 50%
 *   - FULL:    allocate 1MB, access 100% (counter-example)
 */
#define TEST_LAZY_SPARSE      "LAZY_SPARSE"
#define TEST_LAZY_HALF        "LAZY_HALF"
#define TEST_LAZY_FULL        "LAZY_FULL"

/*
 * Buddy System Tests
 * 
 * Test scenarios:
 *   - FRAGMENT:  measure max contiguous allocation after fragmentation
 */
#define TEST_BUDDY_FRAGMENT   "BUDDY_FRAGMENT"

/*
 * mmap Tests
 * 
 * Test scenarios:
 *   - SPARSE:  mmap 1MB, access 1%
 *   - FULL:    mmap 1MB, access 100%
 */
#define TEST_MMAP_SPARSE      "MMAP_SPARSE"
#define TEST_MMAP_FULL        "MMAP_FULL"

/*
 * Scheduler Tests
 * 
 * Format: SCHED_<scenario>_<algorithm>
 * 
 * Scenarios:
 *   - THROUGHPUT:  batch processing throughput
 *   - CONVOY:      convoy effect (short task behind long task)
 *   - FAIRNESS:    CPU time fairness
 *   - RESPONSE:    interactive response time
 * 
 * Algorithms:
 *   - RR, FCFS, SJF, SRTF, PRIORITY, MLFQ, CFS
 */
#define TEST_SCHED_THROUGHPUT_RR       "SCHED_THROUGHPUT_RR"
#define TEST_SCHED_THROUGHPUT_FCFS     "SCHED_THROUGHPUT_FCFS"
#define TEST_SCHED_THROUGHPUT_SJF      "SCHED_THROUGHPUT_SJF"

#define TEST_SCHED_CONVOY_RR           "SCHED_CONVOY_RR"
#define TEST_SCHED_CONVOY_FCFS         "SCHED_CONVOY_FCFS"
#define TEST_SCHED_CONVOY_SRTF         "SCHED_CONVOY_SRTF"

#define TEST_SCHED_FAIRNESS_RR         "SCHED_FAIRNESS_RR"
#define TEST_SCHED_FAIRNESS_PRIORITY   "SCHED_FAIRNESS_PRIORITY"
#define TEST_SCHED_FAIRNESS_CFS        "SCHED_FAIRNESS_CFS"

#define TEST_SCHED_RESPONSE_RR         "SCHED_RESPONSE_RR"
#define TEST_SCHED_RESPONSE_MLFQ       "SCHED_RESPONSE_MLFQ"

/*
 * System Info
 */
#define TEST_SYS_TOTAL_PAGES    "SYS_TOTAL_PAGES"
#define TEST_SYS_FREE_PAGES     "SYS_FREE_PAGES"

/*
 * Test Configuration
 */
#define WARMUP_RUNS     2
#define SCHED_WARMUP_RUNS 1
#define TEST_RUNS       10
#define MAX_RESULT      10

/*
 * Default Test Parameters
 */
#define COW_MEM_SIZE_MB                 5       // 5MB for COW tests
#define LAZY_MEM_SIZE_KB                1024    // 1MB for Lazy tests
#define MMAP_MEM_SIZE_KB                1024    // 1MB for mmap tests
#define SCHED_NTASKS                    10      // Number of tasks for scheduler tests
#define SCHED_FAIRNESS_PROCS            3       // Number of CPU-bound tasks in the fairness scenario
#define SCHED_FAIRNESS_WORK             90000000 // Fixed CPU work per fairness task before completion
#define SCHED_FAIRNESS_POLL_TICKS       4       // Poll interval for progress-based scheduler scenarios
#define SCHED_RESPONSE_HOGS             3       // Number of background CPU hogs before interactive tasks arrive
#define SCHED_RESPONSE_TASKS            6       // Number of interactive tasks in response scenario
#define SCHED_RESPONSE_BACKGROUND_TARGET 60     // Total background rutime before spawning interactive tasks
#define SCHED_RESPONSE_TIMEOUT          120     // Guard timeout in ticks for a single response repetition
#define SCHED_RESPONSE_BASE_WORK        12000000 // Base interactive workload
#define SCHED_RESPONSE_WORK_STEP        2000000 // Extra work added to later interactive tasks

/*
 * Batch sizes for different tests
 * (number of iterations per measurement)
 */
#define BATCH_COW_NO_ACCESS     300
#define BATCH_COW_READONLY      300
#define BATCH_COW_PARTIAL       20
#define BATCH_COW_FULLWRITE     20

#define BATCH_LAZY_SPARSE       2400
#define BATCH_LAZY_HALF         50
#define BATCH_LAZY_FULL         50

#define BATCH_MMAP_SPARSE       2400
#define BATCH_MMAP_FULL         50

#endif /* _PERFTEST_H_ */
