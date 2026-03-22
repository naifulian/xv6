/*
 * perftest.c - Performance Test Suite for xv6-riscv
 * 
 * Testing Branch Version:
 *   - COW enabled
 *   - Lazy Allocation enabled
 *   - Buddy System enabled
 *   - mmap enabled
 * 
 * Output Format:
 *   META:ts=<ticks>:cpus=<n>:branch=testing:sched=<name>
 *   RESULT:<name>:<avg>:<std>:<unit>:<batch>:<runs>
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "perftest.h"

static int results[MAX_RESULT];
static int result_count = 0;

static volatile int g_epoch = 0;

static void
sort_results(void)
{
    for(int i = 0; i < result_count - 1; i++) {
        for(int j = i + 1; j < result_count; j++) {
            if(results[j] < results[i]) {
                int tmp = results[i];
                results[i] = results[j];
                results[j] = tmp;
            }
        }
    }
}

static int
calc_avg(void)
{
    if(result_count < 3) return 0;
    int sum = 0;
    for(int i = 1; i < result_count - 1; i++)
        sum += results[i];
    return sum / (result_count - 2);
}

static int
calc_std(int avg)
{
    if(result_count < 3) return 0;
    int var = 0;
    for(int i = 1; i < result_count - 1; i++) {
        int diff = results[i] - avg;
        var += diff * diff;
    }
    int std = 0;
    int v = var / (result_count - 2);
    while(std * std < v) std++;
    return std;
}

static void
print_meta(void)
{
    int ts = uptime();
    int sched = getscheduler();
    const char *sched_names[] = {
        "RR", "FCFS", "PRIORITY", "SML", "LOTTERY",
        "SJF", "SRTF", "MLFQ", "CFS"
    };
    const char *sname = (sched >= 0 && sched < 9) ? sched_names[sched] : "UNKNOWN";
    printf("META:ts=%d:cpus=1:branch=testing:sched=%s\n", ts, sname);
}

static void
print_result(const char *name, int avg, int std, const char *unit, int batch, int runs)
{
    printf("  %s: %d +/- %d %s (batch=%d, runs=%d)\n", name, avg, std, unit, batch, runs);
    printf("RESULT:%s:%d:%d:%s:%d:%d\n", name, avg, std, unit, batch, runs);
}

static int
measure_max_alloc(void)
{
    int lo = 4096, hi = 16 * 1024 * 1024, best = 0;
    while(lo <= hi) {
        int mid = ((lo + hi) / 2) & ~(4096 - 1);
        char *p = sbrk(mid);
        if(p != (char*)-1) {
            best = mid;
            sbrk(-mid);
            lo = mid + 4096;
        } else {
            hi = mid - 4096;
        }
    }
    return best;
}

static char *mem_base = 0;
static int mem_size = 0;

static void
alloc_mem(int mb)
{
    mem_size = mb * 1024 * 1024;
    mem_base = sbrk(mem_size);
    if(mem_base == (char*)-1) {
        printf("  [ERROR] sbrk(%d MB) failed!\n", mb);
        mem_base = 0;
        mem_size = 0;
        return;
    }
    printf("  [INFO] Allocated %d MB, initializing %d pages...\n", mb, mem_size / 4096);
    for(int i = 0; i < mem_size; i += 4096)
        mem_base[i] = 'A';
}

static void
free_mem(void)
{
    if(mem_base && mem_size > 0) {
        sbrk(-mem_size);
        mem_base = 0;
        mem_size = 0;
    }
}

/*
 * COW Tests
 */
static void
run_cow_test(const char *name, void (*test_fn)(void), int batch)
{
    result_count = 0;
    
    for(int w = 0; w < WARMUP_RUNS; w++) {
        test_fn();
    }
    
    for(int i = 0; i < TEST_RUNS; i++) {
        int start = uptime();
        for(int b = 0; b < batch; b++) {
            test_fn();
        }
        results[result_count++] = uptime() - start;
    }
    
    sort_results();
    int avg = calc_avg();
    int std = calc_std(avg);
    print_result(name, avg, std, "ticks", batch, TEST_RUNS);
}

static void
test_cow_no_access(void)
{
    int pid = fork();
    if(pid == 0) {
        exit(0);
    }
    wait(0);
}

static void
test_cow_readonly(void)
{
    int pid = fork();
    if(pid == 0) {
        volatile int sum = 0;
        for(int i = 0; i < mem_size; i += 4096)
            sum += mem_base[i];
        exit(sum & 1);
    }
    wait(0);
}

static void
test_cow_partial(void)
{
    int pid = fork();
    if(pid == 0) {
        int pages = mem_size / 4096;
        int write_pages = pages * 30 / 100;
        for(int i = 0; i < write_pages; i++) {
            mem_base[i * 4096] = 'B';
        }
        exit(0);
    }
    wait(0);
}

static void
test_cow_fullwrite(void)
{
    int pid = fork();
    if(pid == 0) {
        int pages = mem_size / 4096;
        for(int i = 0; i < pages; i++) {
            mem_base[i * 4096] = 'C';
        }
        exit(0);
    }
    wait(0);
}

/*
 * Lazy Allocation Tests
 */
static void
run_lazy_test(const char *name, int access_percent, int batch)
{
    result_count = 0;
    
    int total_kb = LAZY_MEM_SIZE_KB;
    int access_kb = total_kb * access_percent / 100;
    
    for(int w = 0; w < WARMUP_RUNS; w++) {
        char *p = sbrklazy(total_kb * 1024);
        if(p == (char*)-1) return;
        for(int i = 0; i < access_kb; i += 4) {
            p[i * 1024] = 'X';
        }
        sbrk(-total_kb * 1024);
    }
    
    for(int i = 0; i < TEST_RUNS; i++) {
        int start = uptime();
        for(int b = 0; b < batch; b++) {
            char *p = sbrklazy(total_kb * 1024);
            if(p == (char*)-1) break;
            for(int j = 0; j < access_kb; j += 4) {
                p[j * 1024] = 'X';
            }
            sbrk(-total_kb * 1024);
        }
        results[result_count++] = uptime() - start;
    }
    
    sort_results();
    int avg = calc_avg();
    int std = calc_std(avg);
    print_result(name, avg, std, "ticks", batch, TEST_RUNS);
}

/*
 * Buddy System Test
 */
static void
run_buddy_test(void)
{
    int pids[40];
    int sizes[] = {1, 2, 4, 8, 16};
    int n = 0;
    
    for(int i = 0; i < 40; i++) {
        pids[i] = fork();
        if(pids[i] < 0) {
            printf("  [WARN] fork failed at %d\n", i);
            break;
        }
        if(pids[i] == 0) {
            sbrk(sizes[i % 5] * 4096);
            sleep(50);
            exit(0);
        }
        n++;
    }
    
    for(int i = 0; i < n; i += 2)
        kill(pids[i]);
    sleep(2);
    
    int max_kb = measure_max_alloc() / 1024;
    printf("  %s: %d KB\n", TEST_BUDDY_FRAGMENT, max_kb);
    printf("RESULT:%s:%d:0:kb:1:1\n", TEST_BUDDY_FRAGMENT, max_kb);
    
    for(int i = 1; i < n; i += 2)
        kill(pids[i]);
    while(wait(0) > 0);
}

/*
 * mmap Tests
 */
static void
run_mmap_test(const char *name, int access_percent, int batch)
{
    result_count = 0;
    
    int total_kb = MMAP_MEM_SIZE_KB;
    int access_kb = total_kb * access_percent / 100;
    
    // xv6 mmap currently requires MAP_PRIVATE | MAP_ANONYMOUS.
    char *test_p = mmap(0, 4096, 0x3, 0x22, -1, 0);
    if(test_p == (char*)-1) {
        printf("  [ERROR] mmap not supported, test skipped\n");
        printf("RESULT:%s:-1:0:error:0:0\n", name);
        return;
    }
    munmap(test_p, 4096);
    
    for(int w = 0; w < WARMUP_RUNS; w++) {
        char *p = mmap(0, total_kb * 1024, 0x3, 0x22, -1, 0);
        if(p == (char*)-1) return;
        for(int i = 0; i < access_kb; i += 4) {
            p[i * 1024] = 'X';
        }
        munmap(p, total_kb * 1024);
    }
    
    for(int i = 0; i < TEST_RUNS; i++) {
        int start = uptime();
        for(int b = 0; b < batch; b++) {
            char *p = mmap(0, total_kb * 1024, 0x3, 0x22, -1, 0);
            if(p == (char*)-1) break;
            for(int j = 0; j < access_kb; j += 4) {
                p[j * 1024] = 'X';
            }
            munmap(p, total_kb * 1024);
        }
        results[result_count++] = uptime() - start;
    }
    
    sort_results();
    int avg = calc_avg();
    int std = calc_std(avg);
    print_result(name, avg, std, "ticks", batch, TEST_RUNS);
}

/*
 * Scheduler Tests
 */
static void
cpu_task_heavy(void)
{
    volatile uint64 result = 0;
    for(int i = 0; i < 100000000; i++)
        result += (uint64)i * i;
}

static void
cpu_task_light(void)
{
    volatile uint64 result = 0;
    for(int i = 0; i < 10000000; i++)
        result += i;
}

static void
run_sched_test(const char *name, int sched_class, int ntasks, void (*task_fn)(void))
{
    int saved_class = getscheduler();
    setscheduler(sched_class);
    
    result_count = 0;
    
    for(int run = 0; run < TEST_RUNS; run++) {
        g_epoch = uptime();
        
        int pids[16];
        int n = 0;
        
        for(int i = 0; i < ntasks && i < 16; i++) {
            pids[i] = fork();
            if(pids[i] < 0) break;
            if(pids[i] == 0) {
                task_fn();
                exit(0);
            }
            n++;
        }
        
        for(int i = 0; i < n; i++) {
            int status;
            wait(&status);
        }
        
        int total_time = uptime() - g_epoch;
        results[result_count++] = total_time;
    }
    
    sort_results();
    int avg = calc_avg();
    int std = calc_std(avg);
    print_result(name, avg, std, "ticks", ntasks, TEST_RUNS);
    
    setscheduler(saved_class);
}

static void
print_system_info(void)
{
    printf("\n[SYSTEM INFO]\n");
    uint total = memtotal();
    uint free_pg = memfree();
    printf("  Total physical pages: %d (%d MB)\n", (int)total, (int)(total * 4 / 1024));
    printf("  Free physical pages:  %d (%d MB)\n", (int)free_pg, (int)(free_pg * 4 / 1024));
    printf("RESULT:%s:%d:0:pages:1:1\n", TEST_SYS_TOTAL_PAGES, (int)total);
    printf("RESULT:%s:%d:0:pages:1:1\n", TEST_SYS_FREE_PAGES, (int)free_pg);
}

void
run_cow_tests(void)
{
    printf("\n[COW TESTS - COW enabled]\n");
    
    printf("\n[Allocating %d MB for COW tests...]\n", COW_MEM_SIZE_MB);
    alloc_mem(COW_MEM_SIZE_MB);
    
    if(!mem_base) {
        printf("  [FATAL] Memory allocation failed, skipping COW tests\n");
        return;
    }
    
    print_meta();
    
    printf("\n# Test 1: fork, child exits immediately (no memory access)\n");
    printf("#   testing: copies nothing (best case for COW)\n");
    run_cow_test(TEST_COW_NO_ACCESS, test_cow_no_access, BATCH_COW_NO_ACCESS);
    
    printf("\n# Test 2: fork, child reads all pages (readonly)\n");
    printf("#   testing: copies nothing, shared read\n");
    run_cow_test(TEST_COW_READONLY, test_cow_readonly, BATCH_COW_READONLY);
    
    printf("\n# Test 3: fork, child writes 30%% of pages\n");
    printf("#   testing: copies 30%% on write (COW fault)\n");
    run_cow_test(TEST_COW_PARTIAL, test_cow_partial, BATCH_COW_PARTIAL);
    
    printf("\n# Test 4: fork, child writes all pages (worst case - counter-example)\n");
    printf("#   testing: copies 100%% on write (COW fault overhead)\n");
    run_cow_test(TEST_COW_FULLWRITE, test_cow_fullwrite, BATCH_COW_FULLWRITE);
    
    free_mem();
}

void
run_lazy_tests(void)
{
    printf("\n[LAZY TESTS - Lazy Allocation enabled]\n");
    
    print_meta();
    
    printf("\n# Test 1: access 1%% (%d KB of %d KB) - best case for lazy\n", 
           LAZY_MEM_SIZE_KB / 100, LAZY_MEM_SIZE_KB);
    printf("#   testing: allocates ~3 pages on demand\n");
    run_lazy_test(TEST_LAZY_SPARSE, 1, BATCH_LAZY_SPARSE);
    
    printf("\n# Test 2: access 50%% (%d KB of %d KB) - middle case\n", 
           LAZY_MEM_SIZE_KB / 2, LAZY_MEM_SIZE_KB);
    printf("#   testing: allocates 128 pages on demand\n");
    run_lazy_test(TEST_LAZY_HALF, 50, BATCH_LAZY_HALF);
    
    printf("\n# Test 3: access 100%% (%d KB) - worst case - counter-example\n", 
           LAZY_MEM_SIZE_KB);
    printf("#   testing: allocates 256 pages on demand (page fault overhead)\n");
    run_lazy_test(TEST_LAZY_FULL, 100, BATCH_LAZY_FULL);
}

void
run_buddy_tests(void)
{
    printf("\n[BUDDY TESTS - Buddy System enabled]\n");
    
    print_meta();
    
    printf("\n# Test: measure max contiguous allocation after fragmentation\n");
    printf("#   testing: Buddy System can merge freed blocks\n");
    run_buddy_test();
}

void
run_mmap_tests(void)
{
    printf("\n[MMAP TESTS - mmap enabled]\n");
    
    print_meta();
    
    printf("\n# Test 1: mmap %d KB, access 1%% (best case)\n", MMAP_MEM_SIZE_KB);
    run_mmap_test(TEST_MMAP_SPARSE, 1, BATCH_MMAP_SPARSE);
    
    printf("\n# Test 2: mmap %d KB, access 100%%\n", MMAP_MEM_SIZE_KB);
    run_mmap_test(TEST_MMAP_FULL, 100, BATCH_MMAP_FULL);
}

void
run_sched_tests(void)
{
    printf("\n[SCHED TESTS - 9 Schedulers]\n");
    
    print_meta();
    
    printf("\n# Scenario 1: Throughput (batch processing)\n");
    run_sched_test(TEST_SCHED_THROUGHPUT_RR, 0, SCHED_NTASKS, cpu_task_heavy);
    run_sched_test(TEST_SCHED_THROUGHPUT_FCFS, 1, SCHED_NTASKS, cpu_task_heavy);
    run_sched_test(TEST_SCHED_THROUGHPUT_SJF, 5, SCHED_NTASKS, cpu_task_heavy);
    
    printf("\n# Scenario 2: Convoy Effect (short task behind long task)\n");
    run_sched_test(TEST_SCHED_CONVOY_RR, 0, SCHED_NTASKS, cpu_task_light);
    run_sched_test(TEST_SCHED_CONVOY_FCFS, 1, SCHED_NTASKS, cpu_task_light);
    run_sched_test(TEST_SCHED_CONVOY_SRTF, 6, SCHED_NTASKS, cpu_task_light);
    
    printf("\n# Scenario 3: Fairness\n");
    run_sched_test(TEST_SCHED_FAIRNESS_RR, 0, SCHED_NTASKS, cpu_task_light);
    run_sched_test(TEST_SCHED_FAIRNESS_PRIORITY, 2, SCHED_NTASKS, cpu_task_light);
    run_sched_test(TEST_SCHED_FAIRNESS_CFS, 8, SCHED_NTASKS, cpu_task_light);
    
    printf("\n# Scenario 4: Response Time\n");
    run_sched_test(TEST_SCHED_RESPONSE_RR, 0, SCHED_NTASKS, cpu_task_light);
    run_sched_test(TEST_SCHED_RESPONSE_MLFQ, 7, SCHED_NTASKS, cpu_task_light);
}

int
main(int argc, char *argv[])
{
    printf("\n========================================\n");
    printf("xv6 Performance Test Suite (Testing)\n");
    printf("Config: warmup=%d, runs=%d\n", WARMUP_RUNS, TEST_RUNS);
    printf("========================================\n");
    
    print_system_info();
    
    if(argc > 1) {
        if(strcmp(argv[1], "cow") == 0) {
            run_cow_tests();
        } else if(strcmp(argv[1], "lazy") == 0) {
            run_lazy_tests();
        } else if(strcmp(argv[1], "buddy") == 0) {
            run_buddy_tests();
        } else if(strcmp(argv[1], "mmap") == 0) {
            run_mmap_tests();
        } else if(strcmp(argv[1], "sched") == 0) {
            run_sched_tests();
        } else if(strcmp(argv[1], "all") == 0) {
            run_cow_tests();
            run_lazy_tests();
            run_buddy_tests();
            run_mmap_tests();
            run_sched_tests();
        } else {
            printf("Usage: perftest [cow|lazy|buddy|mmap|sched|all]\n");
        }
    } else {
        run_cow_tests();
        run_lazy_tests();
        run_buddy_tests();
        run_mmap_tests();
        run_sched_tests();
    }
    
    printf("\n========================================\n");
    printf("Tests Complete\n");
    printf("========================================\n");
    exit(0);
}
