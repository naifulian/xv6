/*
 * perftest.h - Performance Test Naming Convention
 * 
 * This header defines standardized test names for both baseline and testing branches.
 * Both branches MUST use the same naming to ensure data comparability.
 */

#ifndef _PERFTEST_H_
#define _PERFTEST_H_

/*
 * COW (Copy-on-Write) Tests
 */
#define TEST_COW_NO_ACCESS    "COW_NO_ACCESS"
#define TEST_COW_READONLY     "COW_READONLY"
#define TEST_COW_PARTIAL      "COW_PARTIAL"
#define TEST_COW_FULLWRITE    "COW_FULLWRITE"

/*
 * Lazy Allocation Tests
 */
#define TEST_LAZY_SPARSE      "LAZY_SPARSE"
#define TEST_LAZY_HALF        "LAZY_HALF"
#define TEST_LAZY_FULL        "LAZY_FULL"

/*
 * Buddy System Tests
 */
#define TEST_BUDDY_FRAGMENT   "BUDDY_FRAGMENT"

/*
 * System Info
 */
#define TEST_SYS_TOTAL_PAGES    "SYS_TOTAL_PAGES"
#define TEST_SYS_FREE_PAGES     "SYS_FREE_PAGES"

/*
 * Test Configuration
 */
#define WARMUP_RUNS     2
#define TEST_RUNS       10
#define MAX_RESULT      10

/*
 * Default Test Parameters
 */
#define COW_MEM_SIZE_MB     5
#define LAZY_MEM_SIZE_KB    1024

/*
 * Batch sizes
 */
#define BATCH_COW_NO_ACCESS     100
#define BATCH_COW_READONLY      100
#define BATCH_COW_PARTIAL       20
#define BATCH_COW_FULLWRITE     20

#define BATCH_LAZY_SPARSE       200
#define BATCH_LAZY_HALF         50
#define BATCH_LAZY_FULL         50

#endif /* _PERFTEST_H_ */
