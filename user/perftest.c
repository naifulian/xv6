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

static volatile uint64 g_sink = 1;

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

static void
print_samples(const char *name)
{
    printf("SAMPLES:%s:%d:", name, result_count);
    for(int i = 0; i < result_count; i++) {
        if(i > 0)
            printf(",");
        printf("%d", results[i]);
    }
    printf("\n");
}

static void
cpu_spin(int loops)
{
    volatile uint64 acc = g_sink + 0x9e3779b97f4a7c15ULL;
    for(int i = 0; i < loops; i++) {
        acc ^= (uint64)(i + 1) * 2654435761ULL;
        acc = (acc << 7) | (acc >> 57);
        acc += 0x85ebca6bULL;
    }
    g_sink ^= acc;
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
    print_samples(name);
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
    print_samples(name);
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
    results[0] = max_kb;
    result_count = 1;
    print_samples(TEST_BUDDY_FRAGMENT);
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
    print_samples(name);
    print_result(name, avg, std, "ticks", batch, TEST_RUNS);
}

/*
 * Scheduler Tests
 */
static int
is_child_pid(int pid, int *pids, int npids)
{
    for(int i = 0; i < npids; i++) {
        if(pids[i] == pid)
            return 1;
    }
    return 0;
}

static void
cleanup_children(int *pids, int npids)
{
    for(int i = 0; i < npids; i++) {
        if(pids[i] > 0)
            kill(pids[i]);
    }
    for(int i = 0; i < npids; i++) {
        if(pids[i] > 0)
            wait(0);
    }
}

static int
measure_throughput_once(void)
{
    static const int workloads[] = {
        300000000, 180000000, 70000000, 20000000, 4000000, 1000000,
    };
    int startfd[2];
    int donefd[2];
    char token = 'S';
    int npids = 0;
    int start;
    int done;
    int sum = 0;
    int completed = 0;

    if(pipe(startfd) < 0)
        return -1;
    if(pipe(donefd) < 0) {
        close(startfd[0]);
        close(startfd[1]);
        return -1;
    }

    start = uptime();

    for(int i = 0; i < 6; i++) {
        int pid = fork();
        if(pid < 0)
            break;
        if(pid == 0) {
            char ch;
            close(startfd[1]);
            close(donefd[0]);
            if(read(startfd[0], &ch, 1) != 1) {
                close(startfd[0]);
                close(donefd[1]);
                exit(1);
            }
            close(startfd[0]);
            cpu_spin(workloads[i]);
            done = uptime() - start;
            write(donefd[1], &done, sizeof(done));
            close(donefd[1]);
            exit(0);
        }
        npids++;
    }

    if(npids == 0) {
        close(startfd[0]);
        close(startfd[1]);
        close(donefd[0]);
        close(donefd[1]);
        return -1;
    }

    close(startfd[0]);
    for(int i = 0; i < npids; i++)
        write(startfd[1], &token, 1);
    close(startfd[1]);

    close(donefd[1]);
    while(read(donefd[0], &done, sizeof(done)) == sizeof(done)) {
        sum += done;
        completed++;
    }
    close(donefd[0]);

    for(int i = 0; i < npids; i++)
        wait(0);

    if(completed == 0)
        return -1;
    return sum / completed;
}

static int
measure_convoy_once(void)
{
    static const int long_work = 220000000;
    static const int short_work[4] = {24000000, 30000000, 36000000, 42000000};
    int pipefd[2];
    int npids = 0;
    int start;
    int done;
    int sum = 0;
    int short_count = 0;

    if(pipe(pipefd) < 0)
        return -1;

    start = uptime();

    int pid = fork();
    if(pid == 0) {
        close(pipefd[0]);
        cpu_spin(long_work);
        close(pipefd[1]);
        exit(0);
    }
    if(pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    npids++;

    for(int i = 0; i < 4; i++) {
        pid = fork();
        if(pid < 0)
            break;
        if(pid == 0) {
            close(pipefd[0]);
            cpu_spin(short_work[i]);
            done = uptime() - start;
            write(pipefd[1], &done, sizeof(done));
            close(pipefd[1]);
            exit(0);
        }
        npids++;
    }

    close(pipefd[1]);
    while(read(pipefd[0], &done, sizeof(done)) == sizeof(done)) {
        sum += done;
        short_count++;
    }
    close(pipefd[0]);

    for(int i = 0; i < npids; i++)
        wait(0);

    if(short_count == 0)
        return -1;
    return sum / short_count;
}

static int
collect_rutime_stats(int *pids, int npids, int *min_rutime, int *max_rutime, int *sum_rutime)
{
    struct pstat table[64];
    int nproc = getptable(table, 64);
    int matched = 0;

    *min_rutime = 0x7fffffff;
    *max_rutime = 0;
    *sum_rutime = 0;

    for(int i = 0; i < nproc; i++) {
        if(is_child_pid(table[i].pid, pids, npids)) {
            matched++;
            if(table[i].rutime < *min_rutime)
                *min_rutime = table[i].rutime;
            if(table[i].rutime > *max_rutime)
                *max_rutime = table[i].rutime;
            *sum_rutime += table[i].rutime;
        }
    }

    if(matched == 0)
        *min_rutime = 0;
    return matched;
}

static int
wait_for_total_rutime(int *pids, int npids, int total_target, int timeout_ticks)
{
    int min_rutime = 0;
    int max_rutime = 0;
    int sum_rutime = 0;
    int deadline = uptime() + timeout_ticks;

    while(uptime() < deadline) {
        int matched = collect_rutime_stats(pids, npids, &min_rutime, &max_rutime, &sum_rutime);
        if(matched == npids && sum_rutime >= total_target)
            return 0;
        sleep(SCHED_FAIRNESS_POLL_TICKS);
    }

    return -1;
}

static int
measure_fairness_once(void)
{
    static const int priorities[SCHED_FAIRNESS_PROCS] = {3, 10, 18};
    int startfd[2];
    int donefd[2];
    int pids[SCHED_FAIRNESS_PROCS];
    char token = 'S';
    int npids = 0;
    int start = uptime();
    int done = 0;
    int min_done = 0x7fffffff;
    int max_done = 0;
    int sum_done = 0;
    int completed = 0;
    int fairness_gap_bp = -1;
    int policy = getscheduler();

    if(pipe(startfd) < 0)
        return -1;
    if(pipe(donefd) < 0) {
        close(startfd[0]);
        close(startfd[1]);
        return -1;
    }

    for(int i = 0; i < SCHED_FAIRNESS_PROCS; i++) {
        int pid = fork();
        if(pid < 0)
            break;
        if(pid == 0) {
            char ch;
            close(startfd[1]);
            close(donefd[0]);
            if(read(startfd[0], &ch, 1) != 1) {
                close(startfd[0]);
                close(donefd[1]);
                exit(1);
            }
            close(startfd[0]);
            cpu_spin(SCHED_FAIRNESS_WORK);
            done = uptime() - start;
            write(donefd[1], &done, sizeof(done));
            close(donefd[1]);
            exit(0);
        }
        pids[npids++] = pid;
        if(policy == 2)
            setpriority(pid, priorities[i]);
    }

    if(npids < SCHED_FAIRNESS_PROCS) {
        close(startfd[0]);
        close(startfd[1]);
        close(donefd[0]);
        close(donefd[1]);
        cleanup_children(pids, npids);
        return -1;
    }

    close(startfd[0]);
    for(int i = 0; i < npids; i++)
        write(startfd[1], &token, 1);
    close(startfd[1]);

    close(donefd[1]);
    while(read(donefd[0], &done, sizeof(done)) == sizeof(done)) {
        completed++;
        sum_done += done;
        if(done < min_done)
            min_done = done;
        if(done > max_done)
            max_done = done;
    }
    close(donefd[0]);

    for(int i = 0; i < npids; i++)
        wait(0);

    if(completed != npids)
        return -1;

    if(completed > 0) {
        int mean_done = sum_done / completed;
        if(mean_done > 0)
            fairness_gap_bp = ((max_done - min_done) * 10000) / mean_done;
    }

    return fairness_gap_bp;
}

static int
measure_response_once(void)
{
    int pipefd[2];
    int pids[SCHED_RESPONSE_HOGS + SCHED_RESPONSE_TASKS];
    int npids = 0;
    int response = -1;
    int start;
    int sum = 0;
    int completed = 0;

    if(pipe(pipefd) < 0)
        return -1;

    for(int i = 0; i < SCHED_RESPONSE_HOGS; i++) {
        int pid = fork();
        if(pid < 0)
            break;
        if(pid == 0) {
            close(pipefd[0]);
            close(pipefd[1]);
            while(1)
                cpu_spin(4000000);
        }
        pids[npids++] = pid;
    }

    if(npids < SCHED_RESPONSE_HOGS) {
        close(pipefd[0]);
        close(pipefd[1]);
        cleanup_children(pids, npids);
        return -1;
    }

    if(wait_for_total_rutime(pids, npids, SCHED_RESPONSE_BACKGROUND_TARGET, SCHED_RESPONSE_TIMEOUT) < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        cleanup_children(pids, npids);
        return -1;
    }

    start = uptime();

    for(int i = 0; i < SCHED_RESPONSE_TASKS; i++) {
        int pid = fork();
        if(pid < 0)
            break;
        if(pid == 0) {
            close(pipefd[0]);
            cpu_spin(SCHED_RESPONSE_BASE_WORK + (i * SCHED_RESPONSE_WORK_STEP));
            response = uptime() - start;
            write(pipefd[1], &response, sizeof(response));
            close(pipefd[1]);
            exit(0);
        }
        pids[npids++] = pid;
    }

    close(pipefd[1]);
    while(read(pipefd[0], &response, sizeof(response)) == sizeof(response)) {
        sum += response;
        completed++;
    }
    close(pipefd[0]);

    cleanup_children(pids, npids);
    if(completed == 0)
        return -1;
    return sum / completed;
}

static void
run_sched_measurement(const char *name, int sched_class, int batch, const char *unit, int (*measure_fn)(void))
{
    int saved_class = getscheduler();
    if(setscheduler(sched_class) < 0) {
        printf("  [ERROR] failed to switch scheduler to %d\n", sched_class);
        printf("RESULT:%s:-1:0:error:%d:0\n", name, batch);
        return;
    }

    result_count = 0;

    for(int w = 0; w < SCHED_WARMUP_RUNS; w++)
        measure_fn();

    for(int run = 0; run < TEST_RUNS; run++) {
        int value = measure_fn();
        if(value < 0)
            break;
        results[result_count++] = value;
    }

    if(result_count < 3) {
        printf("  [ERROR] insufficient scheduler samples\n");
        printf("RESULT:%s:-1:0:error:%d:%d\n", name, batch, result_count);
    } else {
        int avg = 0;
        int std = 0;
        sort_results();
        avg = calc_avg();
        std = calc_std(avg);
        print_samples(name);
        print_result(name, avg, std, unit, batch, result_count);
    }

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
run_memory_tests(void)
{
    run_cow_tests();
    run_lazy_tests();
    run_buddy_tests();
    run_mmap_tests();
}

static void
run_sched_core_tests(void)
{
    printf("\n[SCHED TESTS - Core Scheduler Scenarios]\n");
    print_meta();

    printf("\n# Scenario 1: Throughput (mixed completion time, 6 skewed CPU-bound tasks)\n");
    run_sched_measurement(TEST_SCHED_THROUGHPUT_RR, 0, 6, "ticks", measure_throughput_once);
    run_sched_measurement(TEST_SCHED_THROUGHPUT_FCFS, 1, 6, "ticks", measure_throughput_once);
    run_sched_measurement(TEST_SCHED_THROUGHPUT_SJF, 5, 6, "ticks", measure_throughput_once);

    printf("\n# Scenario 2: Convoy Effect (1 long + 4 short tasks, average short completion time)\n");
    run_sched_measurement(TEST_SCHED_CONVOY_RR, 0, 4, "ticks", measure_convoy_once);
    run_sched_measurement(TEST_SCHED_CONVOY_FCFS, 1, 4, "ticks", measure_convoy_once);
    run_sched_measurement(TEST_SCHED_CONVOY_SRTF, 6, 4, "ticks", measure_convoy_once);
}

static void
run_sched_fairness_tests(void)
{
    printf("\n[SCHED TESTS - Fairness Scenarios]\n");
    print_meta();

    printf("\n# Scenario 3: Fairness (normalized CPU-service gap at fixed service budget, lower is better)\n");
    run_sched_measurement(TEST_SCHED_FAIRNESS_RR, 0, SCHED_FAIRNESS_PROCS, "bp", measure_fairness_once);
    run_sched_measurement(TEST_SCHED_FAIRNESS_PRIORITY, 2, SCHED_FAIRNESS_PROCS, "bp", measure_fairness_once);
    run_sched_measurement(TEST_SCHED_FAIRNESS_CFS, 8, SCHED_FAIRNESS_PROCS, "bp", measure_fairness_once);
}

static void
run_sched_response_tests(void)
{
    printf("\n[SCHED TESTS - Response Scenarios]\n");
    print_meta();

    printf("\n# Scenario 4: Response Time (%d CPU hogs warmed by service budget + %d interactive tasks)\n", SCHED_RESPONSE_HOGS, SCHED_RESPONSE_TASKS);
    run_sched_measurement(TEST_SCHED_RESPONSE_RR, 0, SCHED_RESPONSE_TASKS, "ticks", measure_response_once);
    run_sched_measurement(TEST_SCHED_RESPONSE_MLFQ, 7, SCHED_RESPONSE_TASKS, "ticks", measure_response_once);
}

void
run_sched_tests(void)
{
    run_sched_core_tests();
    run_sched_fairness_tests();
    run_sched_response_tests();
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
        } else if(strcmp(argv[1], "memory") == 0) {
            run_memory_tests();
        } else if(strcmp(argv[1], "sched") == 0) {
            run_sched_tests();
        } else if(strcmp(argv[1], "sched_core") == 0) {
            run_sched_core_tests();
        } else if(strcmp(argv[1], "sched_fairness") == 0) {
            run_sched_fairness_tests();
        } else if(strcmp(argv[1], "sched_response") == 0) {
            run_sched_response_tests();
        } else if(strcmp(argv[1], "all") == 0) {
            run_memory_tests();
            run_sched_tests();
        } else {
            printf("Usage: perftest [cow|lazy|buddy|mmap|memory|sched|sched_core|sched_fairness|sched_response|all]\n");
        }
    } else {
        run_memory_tests();
        run_sched_tests();
    }
    
    printf("\n========================================\n");
    printf("Tests Complete\n");
    printf("========================================\n");
    exit(0);
}
