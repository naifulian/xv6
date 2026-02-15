// perf_test.c - Performance benchmark for xv6 optimizations
// Enhanced with multiple runs, averaging, and CSV output
// Tests: COW, Lazy allocation, Scheduler algorithms

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_CHILDREN 20
#define MAX_PSTAT 64
#define MAX_RUNS 5

static char *sched_names[] = {
  "DEFAULT", "FCFS", "PRIORITY", "SML", "LOTTERY"
};

static int csv_mode = 0;
static int runs = 3;

void
print_separator(void)
{
  if(!csv_mode)
    printf("------------------------------------------------------------\n");
}

// Structure to hold benchmark results
struct bench_result {
  char name[32];
  int values[MAX_RUNS];
  int count;
  int min;
  int max;
  int avg;
};

void
init_result(struct bench_result *r, const char *name)
{
  int i;
  for(i = 0; i < sizeof(r->name) - 1 && name[i]; i++) {
    r->name[i] = name[i];
  }
  r->name[i] = '\0';
  r->count = 0;
  r->min = 0x7FFFFFFF;
  r->max = 0;
  r->avg = 0;
}

void
add_value(struct bench_result *r, int val)
{
  if(r->count < MAX_RUNS) {
    r->values[r->count++] = val;
    if(val < r->min) r->min = val;
    if(val > r->max) r->max = val;
  }
}

void
calc_avg(struct bench_result *r)
{
  int sum = 0;
  for(int i = 0; i < r->count; i++) {
    sum += r->values[i];
  }
  r->avg = r->count > 0 ? sum / r->count : 0;
}

void
print_result(struct bench_result *r)
{
  if(csv_mode) {
    printf("%s,%d,%d,%d\n", r->name, r->min, r->max, r->avg);
  } else {
    printf("  %-24s min=%-4d max=%-4d avg=%-4d (%d runs)\n", 
           r->name, r->min, r->max, r->avg, r->count);
  }
}

// Test: COW fork+exec (Best case - no write)
void
test_cow_exec_only(int count)
{
  struct bench_result time_result, cow_result;
  init_result(&time_result, "cow_exec_time");
  init_result(&cow_result, "cow_exec_copies");

  if(!csv_mode) {
    printf("\n=== COW Test: fork+exec (Best Case) ===\n");
    printf("Creating %d processes that fork+exec immediately\n", count);
    printf("Running %d iterations...\n", runs);
  }

  for(int run = 0; run < runs; run++) {
    struct memstat mem_before, mem_after;
    getmemstat(&mem_before);

    uint64 start = uptime();

    for(int i = 0; i < count; i++) {
      int pid = fork();
      if(pid == 0) {
        char *argv[] = {"echo", "cow", 0};
        exec("echo", argv);
        exit(1);
      } else if(pid < 0) {
        break;
      }
    }

    for(int i = 0; i < count; i++) {
      wait(0);
    }

    uint64 end = uptime();
    getmemstat(&mem_after);

    add_value(&time_result, (int)(end - start));
    add_value(&cow_result, mem_after.cow_copy_pages - mem_before.cow_copy_pages);
  }

  calc_avg(&time_result);
  calc_avg(&cow_result);

  if(!csv_mode) {
    printf("\nResults:\n");
  }
  print_result(&time_result);
  print_result(&cow_result);
}

// Test: COW with partial write
void
test_cow_partial_write(int count, int write_percent)
{
  struct bench_result time_result, cow_result;
  init_result(&time_result, "cow_write_time");
  init_result(&cow_result, "cow_write_copies");

  if(!csv_mode) {
    printf("\n=== COW Test: Partial Write (%d%%) ===\n", write_percent);
    printf("Running %d iterations...\n", runs);
  }

  int pages = 100;

  for(int run = 0; run < runs; run++) {
    char *data = sbrk(pages * 4096);
    if(data == (char*)-1) {
      printf("sbrk failed\n");
      return;
    }

    for(int i = 0; i < pages; i++) {
      data[i * 4096] = 'A' + (i % 26);
    }

    struct memstat mem_before, mem_after;
    getmemstat(&mem_before);

    int write_pages = pages * write_percent / 100;
    uint64 start = uptime();

    for(int i = 0; i < count; i++) {
      int pid = fork();
      if(pid == 0) {
        for(int j = 0; j < write_pages; j++) {
          data[j * 4096] = 'B';
        }
        exit(0);
      } else if(pid < 0) {
        break;
      }
    }

    for(int i = 0; i < count; i++) {
      wait(0);
    }

    uint64 end = uptime();
    getmemstat(&mem_after);

    add_value(&time_result, (int)(end - start));
    add_value(&cow_result, mem_after.cow_copy_pages - mem_before.cow_copy_pages);

    sbrk(-pages * 4096);
  }

  calc_avg(&time_result);
  calc_avg(&cow_result);

  if(!csv_mode) {
    printf("\nResults:\n");
  }
  print_result(&time_result);
  print_result(&cow_result);
}

// Test: Lazy allocation - sparse access
void
test_lazy_sparse(uint alloc_kb, uint access_kb)
{
  struct bench_result lazy_result, saved_result;
  init_result(&lazy_result, "lazy_allocs");
  init_result(&saved_result, "lazy_saved_pct");

  if(!csv_mode) {
    printf("\n=== Lazy Allocation Test: Sparse Access ===\n");
    printf("Allocating %d KB, accessing %d KB\n", alloc_kb, access_kb);
    printf("Running %d iterations...\n", runs);
  }

  for(int run = 0; run < runs; run++) {
    struct memstat mem_before, mem_after;
    getmemstat(&mem_before);

    char *addr = sbrk(alloc_kb * 1024);
    if(addr == (char*)-1) {
      printf("sbrk failed\n");
      return;
    }

    getmemstat(&mem_after);
    int pages_before = mem_after.free_pages;

    for(uint i = 0; i < access_kb * 1024; i += 4096) {
      addr[i] = 'X';
    }

    getmemstat(&mem_after);
    int pages_used = pages_before - mem_after.free_pages;
    int lazy_allocs = mem_after.lazy_allocs - mem_before.lazy_allocs;

    int requested_pages = alloc_kb / 4;
    int saved = requested_pages > 0 ? (requested_pages - pages_used) * 100 / requested_pages : 0;

    add_value(&lazy_result, lazy_allocs);
    add_value(&saved_result, saved);

    sbrk(-alloc_kb * 1024);
  }

  calc_avg(&lazy_result);
  calc_avg(&saved_result);

  if(!csv_mode) {
    printf("\nResults:\n");
  }
  print_result(&lazy_result);
  print_result(&saved_result);
}

// Test: Scheduler - context switch overhead
void
test_sched_switch_overhead(int num_procs)
{
  if(!csv_mode) {
    printf("\n=== Scheduler Test: Context Switch Overhead ===\n");
    printf("Creating %d CPU-bound processes\n", num_procs);
    printf("Running %d iterations per scheduler...\n", runs);
  }

  int original_sched = getscheduler();

  for(int sched = 0; sched <= 4; sched++) {
    struct bench_result r;
    char name[32];
    int j;
    for(j = 0; j < sizeof(name) - 1 && sched_names[sched][j]; j++) {
      name[j] = sched_names[sched][j];
    }
    name[j] = '\0';
    init_result(&r, name);

    for(int run = 0; run < runs; run++) {
      setscheduler(sched);

      uint64 start = uptime();

      for(int i = 0; i < num_procs; i++) {
        int pid = fork();
        if(pid == 0) {
          volatile int sum = 0;
          for(volatile int j = 0; j < 100000; j++) sum += j;
          exit(0);
        } else if(pid < 0) {
          break;
        }
      }

      for(int i = 0; i < num_procs; i++) {
        wait(0);
      }

      uint64 end = uptime();
      add_value(&r, (int)(end - start));
    }

    calc_avg(&r);
    print_result(&r);
  }

  setscheduler(original_sched);
}

// Test: Scheduler - priority response
void
test_sched_priority_response(void)
{
  struct bench_result r;
  init_result(&r, "priority_response");

  if(!csv_mode) {
    printf("\n=== Scheduler Test: Priority Response ===\n");
    printf("Running %d iterations...\n", runs);
  }

  int original_sched = getscheduler();

  for(int run = 0; run < runs; run++) {
    setscheduler(2);  // PRIORITY scheduler

    uint64 start = uptime();

    // 2 high priority
    for(int i = 0; i < 2; i++) {
      int pid = fork();
      if(pid == 0) {
        setpriority(getpid(), 1);
        volatile int sum = 0;
        for(volatile int j = 0; j < 50000; j++) sum += j;
        exit(0);
      }
    }

    // 8 low priority
    for(int i = 0; i < 8; i++) {
      int pid = fork();
      if(pid == 0) {
        setpriority(getpid(), 20);
        volatile int sum = 0;
        for(volatile int j = 0; j < 500000; j++) sum += j;
        exit(0);
      }
    }

    for(int i = 0; i < 10; i++) {
      wait(0);
    }

    uint64 end = uptime();
    add_value(&r, (int)(end - start));
  }

  calc_avg(&r);

  if(!csv_mode) {
    printf("\nResults:\n");
  }
  print_result(&r);

  setscheduler(original_sched);
}

// Test: Scheduler - lottery fairness
void
test_sched_lottery_fairness(void)
{
  if(!csv_mode) {
    printf("\n=== Scheduler Test: Lottery Fairness ===\n");
    printf("Creating 3 processes with tickets 1:10:100\n");
    printf("Expected CPU ratio: ~0.9%% : ~9%% : ~90%%\n");
  }

  int original_sched = getscheduler();
  setscheduler(4);  // LOTTERY scheduler

  int pids[3];
  int tickets[3] = {1, 10, 100};

  for(int i = 0; i < 3; i++) {
    pids[i] = fork();
    if(pids[i] == 0) {
      volatile int sum = 0;
      uint64 end = uptime() + 100;
      while(uptime() < end) {
        for(volatile int j = 0; j < 1000; j++) sum += j;
      }
      exit(0);
    }
  }

  sleep(50);

  struct pstat ps[MAX_PSTAT];
  int n = getptable(ps, MAX_PSTAT);

  if(!csv_mode) {
    printf("\nProcess statistics:\n");
    printf("PID      Tickets  rutime   retime   stime\n");
  }

  for(int i = 0; i < n; i++) {
    for(int j = 0; j < 3; j++) {
      if(ps[i].pid == pids[j]) {
        if(csv_mode) {
          printf("lottery_%d,%d,%d,%d,%d\n", tickets[j], tickets[j], 
                 ps[i].rutime, ps[i].retime, ps[i].stime);
        } else {
          printf("%-8d %-8d %-8d %-8d %-8d\n", 
                 ps[i].pid, tickets[j], ps[i].rutime, ps[i].retime, ps[i].stime);
        }
      }
    }
  }

  for(int i = 0; i < 3; i++) {
    wait(0);
  }

  setscheduler(original_sched);
}

// Test: Combined - fork with memory + lazy
void
test_combined_fork_lazy(int fork_count, int mem_kb)
{
  struct bench_result time_result, lazy_result;
  init_result(&time_result, "combined_time");
  init_result(&lazy_result, "combined_lazy");

  if(!csv_mode) {
    printf("\n=== Combined Test: fork + Lazy Allocation ===\n");
    printf("%d forks, each with %d KB memory\n", fork_count, mem_kb);
    printf("Running %d iterations...\n", runs);
  }

  for(int run = 0; run < runs; run++) {
    struct memstat mem_before, mem_after;
    getmemstat(&mem_before);

    uint64 start = uptime();

    for(int i = 0; i < fork_count; i++) {
      int pid = fork();
      if(pid == 0) {
        char *data = sbrk(mem_kb * 1024);
        if(data != (char*)-1) {
          data[0] = 'X';
          data[4096] = 'Y';
        }
        exit(0);
      } else if(pid < 0) {
        break;
      }
    }

    for(int i = 0; i < fork_count; i++) {
      wait(0);
    }

    uint64 end = uptime();
    getmemstat(&mem_after);

    add_value(&time_result, (int)(end - start));
    add_value(&lazy_result, mem_after.lazy_allocs - mem_before.lazy_allocs);
  }

  calc_avg(&time_result);
  calc_avg(&lazy_result);

  if(!csv_mode) {
    printf("\nResults:\n");
  }
  print_result(&time_result);
  print_result(&lazy_result);
}

// Memory benchmark - comprehensive
void
test_memory_comprehensive(void)
{
  if(!csv_mode) {
    printf("\n=== Memory Management Comprehensive Test ===\n");
  }

  struct memstat ms;
  getmemstat(&ms);

  if(csv_mode) {
    printf("mem_total,%d\n", ms.total_pages);
    printf("mem_free,%d\n", ms.free_pages);
    printf("mem_used,%d\n", ms.total_pages - ms.free_pages);
    printf("mem_buddy_merges,%d\n", ms.buddy_merges);
    printf("mem_buddy_splits,%d\n", ms.buddy_splits);
    printf("mem_cow_faults,%d\n", ms.cow_faults);
    printf("mem_lazy_allocs,%d\n", ms.lazy_allocs);
    printf("mem_cow_copies,%d\n", ms.cow_copy_pages);
  } else {
    printf("Memory Statistics:\n");
    printf("  Total pages:    %d\n", ms.total_pages);
    printf("  Free pages:     %d\n", ms.free_pages);
    printf("  Used pages:     %d\n", ms.total_pages - ms.free_pages);
    printf("  Buddy merges:   %d\n", ms.buddy_merges);
    printf("  Buddy splits:   %d\n", ms.buddy_splits);
    printf("  COW faults:     %d\n", ms.cow_faults);
    printf("  Lazy allocs:    %d\n", ms.lazy_allocs);
    printf("  COW copies:     %d\n", ms.cow_copy_pages);
  }
}

void
print_usage(void)
{
  printf("Usage: perf_test [options] <test> [args]\n");
  printf("\nOptions:\n");
  printf("  -c          CSV output mode\n");
  printf("  -r <n>      Run each test n times (default 3)\n");
  printf("\nTests:\n");
  printf("  cow-exec <count>          COW fork+exec test\n");
  printf("  cow-write <count> <%%>      COW partial write test\n");
  printf("  lazy <alloc_kb> <access_kb>  Lazy allocation test\n");
  printf("  sched-switch <count>      Scheduler switch overhead\n");
  printf("  sched-priority            Priority response test\n");
  printf("  sched-lottery             Lottery fairness test\n");
  printf("  combined <forks> <mem_kb> Combined fork+lazy test\n");
  printf("  memory                    Memory statistics\n");
  printf("  all                       Run all tests\n");
}

int
main(int argc, char *argv[])
{
  int arg_idx = 1;

  // Parse options
  while(arg_idx < argc && argv[arg_idx][0] == '-') {
    if(strcmp(argv[arg_idx], "-c") == 0) {
      csv_mode = 1;
      arg_idx++;
    } else if(strcmp(argv[arg_idx], "-r") == 0 && arg_idx + 1 < argc) {
      runs = atoi(argv[++arg_idx]);
      if(runs < 1) runs = 1;
      if(runs > MAX_RUNS) runs = MAX_RUNS;
      arg_idx++;
    } else if(strcmp(argv[arg_idx], "-h") == 0) {
      print_usage();
      exit(0);
    } else {
      arg_idx++;
    }
  }

  if(arg_idx >= argc) {
    print_usage();
    exit(1);
  }

  char *test = argv[arg_idx++];

  if(csv_mode) {
    printf("test,min,max,avg\n");
  }

  if(strcmp(test, "cow-exec") == 0) {
    int count = arg_idx < argc ? atoi(argv[arg_idx]) : 5;
    test_cow_exec_only(count);
  }
  else if(strcmp(test, "cow-write") == 0) {
    int count = arg_idx < argc ? atoi(argv[arg_idx]) : 3;
    int percent = arg_idx + 1 < argc ? atoi(argv[arg_idx + 1]) : 30;
    test_cow_partial_write(count, percent);
  }
  else if(strcmp(test, "lazy") == 0) {
    int alloc = arg_idx < argc ? atoi(argv[arg_idx]) : 1024;
    int access = arg_idx + 1 < argc ? atoi(argv[arg_idx + 1]) : 10;
    test_lazy_sparse(alloc, access);
  }
  else if(strcmp(test, "sched-switch") == 0) {
    int count = arg_idx < argc ? atoi(argv[arg_idx]) : 5;
    test_sched_switch_overhead(count);
  }
  else if(strcmp(test, "sched-priority") == 0) {
    test_sched_priority_response();
  }
  else if(strcmp(test, "sched-lottery") == 0) {
    test_sched_lottery_fairness();
  }
  else if(strcmp(test, "combined") == 0) {
    int forks = arg_idx < argc ? atoi(argv[arg_idx]) : 5;
    int mem = arg_idx + 1 < argc ? atoi(argv[arg_idx + 1]) : 256;
    test_combined_fork_lazy(forks, mem);
  }
  else if(strcmp(test, "memory") == 0) {
    test_memory_comprehensive();
  }
  else if(strcmp(test, "all") == 0) {
    if(!csv_mode) {
      printf("\n========== Running All Performance Tests ==========\n");
      printf("Each test runs %d times, showing min/max/avg\n", runs);
    }
    test_cow_exec_only(5);
    test_cow_partial_write(3, 30);
    test_lazy_sparse(1024, 10);
    test_sched_switch_overhead(5);
    test_sched_priority_response();
    test_sched_lottery_fairness();
    test_combined_fork_lazy(5, 256);
    test_memory_comprehensive();
    if(!csv_mode) {
      printf("\n========== All Tests Complete ==========\n");
    }
  }
  else {
    print_usage();
    exit(1);
  }

  exit(0);
}
