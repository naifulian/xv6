#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define WARMUP_RUNS 2
#define TEST_RUNS 10
#define MAX_RESULT 10

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
print_result(const char *name, int avg, int std, const char *unit)
{
    printf("  %s: %d +/- %d %s\n", name, avg, std, unit);
    printf("RESULT:%s:%d:%d:%s\n", name, avg, std, unit);
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
    printf("  [DEBUG] Allocated %d MB at %p, writing to each page...\n", mb, mem_base);
    for(int i = 0; i < mem_size; i += 4096)
        mem_base[i] = 'A';
    printf("  [DEBUG] Memory initialized, %d pages written\n", mem_size / 4096);
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
    print_result(name, avg, std, "ticks");
}

static void
test_cow_1_no_access(void)
{
    int pid = fork();
    if(pid == 0) {
        exit(0);
    }
    wait(0);
}

static void
test_cow_2_readonly(void)
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
test_cow_3_partial(void)
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
test_cow_4_fullwrite(void)
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

static void
run_lazy_test(const char *name, int access_percent, int batch)
{
    result_count = 0;
    
    int total_kb = 1024;
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
    print_result(name, avg, std, "ticks");
}

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
    printf("  M8: %d KB\n", max_kb);
    printf("RESULT:M8:%d:0:kb\n", max_kb);
    
    for(int i = 1; i < n; i += 2)
        kill(pids[i]);
    while(wait(0) > 0);
}

static void
run_mmap_test(const char *name, int access_percent, int batch)
{
    printf("  [DEBUG] Testing %s...\n", name);
    
    result_count = 0;
    
    int total_kb = 1024;
    int access_kb = total_kb * access_percent / 100;
    
    char *test_p = mmap(0, 4096, 0x1, 0x20, -1, 0);
    if(test_p == (char*)-1) {
        printf("  [ERROR] mmap failed, test skipped\n");
        printf("RESULT:%s:-1:0:error\n", name);
        return;
    }
    munmap(test_p, 4096);
    
    for(int w = 0; w < WARMUP_RUNS; w++) {
        char *p = mmap(0, total_kb * 1024, 0x1, 0x20, -1, 0);
        if(p == (char*)-1) return;
        for(int i = 0; i < access_kb; i += 4) {
            p[i * 1024] = 'X';
        }
        munmap(p, total_kb * 1024);
    }
    
    for(int i = 0; i < TEST_RUNS; i++) {
        int start = uptime();
        for(int b = 0; b < batch; b++) {
            char *p = mmap(0, total_kb * 1024, 0x1, 0x20, -1, 0);
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
    print_result(name, avg, std, "ticks");
}

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
run_sched_test(const char *name, int sched_class, int ntasks, void (*task_fn)(void), int is_long_short)
{
    int saved_class = getscheduler();
    setscheduler(sched_class);
    
    result_count = 0;
    
    for(int run = 0; run < TEST_RUNS; run++) {
        g_epoch = uptime();
        
        int pids[10];
        int n = 0;
        
        for(int i = 0; i < ntasks && i < 10; i++) {
            pids[i] = fork();
            if(pids[i] < 0) break;
            if(pids[i] == 0) {
                task_fn();
                exit(0);
            }
            n++;
        }
        
        int total_time = 0;
        for(int i = 0; i < n; i++) {
            int status;
            wait(&status);
        }
        
        total_time = uptime() - g_epoch;
        results[result_count++] = total_time;
    }
    
    sort_results();
    int avg = calc_avg();
    int std = calc_std(avg);
    print_result(name, avg, std, "ticks");
    
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
    printf("RESULT:sys_total_pages:%d:0:pages\n", (int)total);
    printf("RESULT:sys_free_pages:%d:0:pages\n", (int)free_pg);
}

void
run_cow_tests(void)
{
    printf("\n[COW TESTS - COW enabled]\n");
    printf("# Note: Testing uses COW (copy-on-write)\n");
    
    printf("\n[Allocating 5MB for COW tests...]\n");
    alloc_mem(5);
    
    if(!mem_base) {
        printf("  [FATAL] Memory allocation failed, skipping COW tests\n");
        return;
    }
    
    printf("\n# M1: fork, child exits immediately (no memory access)\n");
    printf("#   testing: copies nothing (best case)\n");
    run_cow_test("M1", test_cow_1_no_access, 100);
    
    printf("\n# M2: fork, child reads all pages (readonly)\n");
    printf("#   testing: copies nothing, just reads\n");
    run_cow_test("M2", test_cow_2_readonly, 100);
    
    printf("\n# M3: fork, child writes 30%% of pages\n");
    printf("#   testing: copies 30%% on write (COW fault)\n");
    run_cow_test("M3", test_cow_3_partial, 20);
    
    printf("\n# M4: fork, child writes all pages (worst case - counter-example)\n");
    printf("#   testing: copies 100%% on write (COW fault overhead)\n");
    run_cow_test("M4", test_cow_4_fullwrite, 20);
    
    free_mem();
}

void
run_lazy_tests(void)
{
    printf("\n[LAZY TESTS - Lazy Allocation enabled]\n");
    printf("# Note: Testing uses Lazy Allocation (demand paging)\n");
    printf("# Testing sbrklazy(1MB) with different access patterns\n");
    
    printf("\n# M5: access 1%% (10KB of 1MB) - best case for lazy\n");
    printf("#   testing: allocates ~3 pages on demand\n");
    run_lazy_test("M5", 1, 200);
    
    printf("\n# M6: access 50%% (512KB of 1MB) - middle case\n");
    printf("#   testing: allocates 128 pages on demand\n");
    run_lazy_test("M6", 50, 50);
    
    printf("\n# M7: access 100%% (1MB) - worst case - counter-example\n");
    printf("#   testing: allocates 256 pages on demand (page fault overhead)\n");
    run_lazy_test("M7", 100, 50);
}

void
run_buddy_tests(void)
{
    printf("\n[BUDDY TESTS - Buddy System enabled]\n");
    printf("# Note: Testing uses Buddy System allocator\n");
    printf("# Measuring max contiguous allocation after fragmentation\n");
    
    run_buddy_test();
}

void
run_mmap_tests(void)
{
    printf("\n[MMAP TESTS - mmap enabled]\n");
    printf("# Note: Testing supports mmap anonymous mapping\n");
    printf("# Testing mmap(1MB) with different access patterns\n");
    
    printf("\n# MMAP-1: mmap 1MB, access 1%% (best case)\n");
    run_mmap_test("MMAP_1_sparse", 1, 50);
    
    printf("\n# MMAP-2: mmap 1MB, access 100%%\n");
    run_mmap_test("MMAP_2_full", 100, 50);
}

void
run_sched_tests(void)
{
    printf("\n[SCHED TESTS - 9 Schedulers]\n");
    printf("# Note: Testing all 9 schedulers\n");
    
    printf("\n# SCHED-1: batch processing\n");
    run_sched_test("SCHED_1_RR", 0, 10, cpu_task_heavy, 0);
    run_sched_test("SCHED_1_FCFS", 1, 10, cpu_task_heavy, 0);
    run_sched_test("SCHED_1_SJF", 5, 10, cpu_task_heavy, 0);
    
    printf("\n# SCHED-2: convoy effect\n");
    run_sched_test("SCHED_2_RR", 0, 10, cpu_task_light, 1);
    run_sched_test("SCHED_2_FCFS", 1, 10, cpu_task_light, 1);
    run_sched_test("SCHED_2_SRTF", 6, 10, cpu_task_light, 1);
    
    printf("\n# SCHED-3: priority\n");
    run_sched_test("SCHED_3_RR", 0, 10, cpu_task_light, 0);
    run_sched_test("SCHED_3_PRT", 2, 10, cpu_task_light, 0);
    
    printf("\n# SCHED-4: MLFQ interactive\n");
    run_sched_test("SCHED_4_RR", 0, 10, cpu_task_light, 0);
    run_sched_test("SCHED_4_MLFQ", 7, 10, cpu_task_light, 0);
    
    printf("\n# SCHED-5: CFS fairness\n");
    run_sched_test("SCHED_5_RR", 0, 10, cpu_task_light, 0);
    run_sched_test("SCHED_5_CFS", 8, 10, cpu_task_light, 0);
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
