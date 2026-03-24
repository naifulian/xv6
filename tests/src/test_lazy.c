// test_lazy.c - Lazy allocation unit test implementation
// Strengthened to exercise the real lazy-allocation interface.
#include "../include/test_lazy.h"

#define PAGE_SIZE 4096

static void
read_memstat(struct memstat *ms)
{
    if(getmemstat(ms) < 0) {
        printf("  FAIL: getmemstat failed\n");
        exit(1);
    }
}

static int
free_page_delta(struct memstat *before, struct memstat *after)
{
    return (int)before->free_pages - (int)after->free_pages;
}

static int
lazy_fault_delta(struct memstat *before, struct memstat *after)
{
    return (int)after->lazy_allocs - (int)before->lazy_allocs;
}

static void
assert_exact(const char *label, int actual, int expected)
{
    if(actual != expected) {
        printf("  FAIL: %s = %d (expected %d)\n", label, actual, expected);
        exit(1);
    }
}

static char *
lazy_alloc(int size)
{
    char *addr = sbrklazy(size);
    if(addr == (char*)-1) {
        printf("  FAIL: sbrklazy(%d) failed\n", size);
        exit(1);
    }
    return addr;
}

static void
lazy_free(int size)
{
    if(sbrk(-size) == (char*)-1) {
        printf("  FAIL: sbrk(-%d) failed during cleanup\n", size);
        exit(1);
    }
}

static void
assert_lazy_request_is_deferred(struct memstat *before, struct memstat *after)
{
    assert_exact("pages used immediately after sbrklazy", free_page_delta(before, after), 0);
    assert_exact("lazy faults immediately after sbrklazy", lazy_fault_delta(before, after), 0);
}

void
test_lazy_large_alloc_small_access(void)
{
    printf("[TEST] lazy_large_alloc_small_access\n");

    struct memstat before, after;
    int request_size = 1024 * 1024;
    int access_bytes = 10 * 1024;
    int expected_pages = (access_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    read_memstat(&before);
    char *addr = lazy_alloc(request_size);
    printf("  OK: sbrklazy 1MB = %p\n", addr);

    read_memstat(&after);
    assert_lazy_request_is_deferred(&before, &after);
    printf("  OK: request reserved virtual space without physical allocation\n");

    read_memstat(&before);
    memset(addr, 'A', access_bytes);
    read_memstat(&after);

    int lazy_faults = lazy_fault_delta(&before, &after);
    int pages_used = free_page_delta(&before, &after);
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_faults, pages_used);

    if(addr[0] != 'A' || addr[access_bytes - 1] != 'A') {
        printf("  FAIL: data verification failed\n");
        lazy_free(request_size);
        exit(1);
    }

    assert_exact("pages allocated for 10KB access", pages_used, expected_pages);
    assert_exact("lazy faults for 10KB access", lazy_faults, expected_pages);
    printf("  PASS: Lazy allocated exactly %d pages for 10KB access\n", pages_used);

    lazy_free(request_size);
}

void
test_lazy_large_alloc_full_access(void)
{
    printf("[TEST] lazy_large_alloc_full_access\n");

    struct memstat before, after;
    int request_size = 1024 * 1024;
    int expected_pages = 256;

    read_memstat(&before);
    char *addr = lazy_alloc(request_size);
    printf("  OK: sbrklazy 1MB = %p\n", addr);

    read_memstat(&after);
    assert_lazy_request_is_deferred(&before, &after);
    printf("  OK: request reserved virtual space without physical allocation\n");

    read_memstat(&before);
    for(int i = 0; i < expected_pages; i++) {
        addr[i * PAGE_SIZE] = 'B' + (i % 26);
    }
    read_memstat(&after);

    int lazy_faults = lazy_fault_delta(&before, &after);
    int pages_used = free_page_delta(&before, &after);
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_faults, pages_used);

    if(addr[0] != 'B' || addr[(expected_pages - 1) * PAGE_SIZE] != 'B' + ((expected_pages - 1) % 26)) {
        printf("  FAIL: data verification failed\n");
        lazy_free(request_size);
        exit(1);
    }

    assert_exact("pages allocated for full 1MB access", pages_used, expected_pages);
    assert_exact("lazy faults for full 1MB access", lazy_faults, expected_pages);
    printf("  PASS: Full access faulted every requested page\n");

    lazy_free(request_size);
}

void
test_lazy_sequential_access(void)
{
    printf("[TEST] lazy_sequential_access\n");

    struct memstat before, after;
    int request_size = 2 * 1024 * 1024;
    int expected_pages = 512;

    read_memstat(&before);
    char *addr = lazy_alloc(request_size);
    printf("  OK: sbrklazy 2MB = %p\n", addr);

    read_memstat(&after);
    assert_lazy_request_is_deferred(&before, &after);
    printf("  OK: request reserved virtual space without physical allocation\n");

    read_memstat(&before);
    for(int i = 0; i < expected_pages; i++) {
        addr[i * PAGE_SIZE] = 'C' + (i % 26);
    }
    read_memstat(&after);

    int lazy_faults = lazy_fault_delta(&before, &after);
    int pages_used = free_page_delta(&before, &after);
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_faults, pages_used);

    for(int i = 0; i < expected_pages; i++) {
        if(addr[i * PAGE_SIZE] != 'C' + (i % 26)) {
            printf("  FAIL: data error at page %d\n", i);
            lazy_free(request_size);
            exit(1);
        }
    }

    assert_exact("pages allocated for sequential 2MB access", pages_used, expected_pages);
    assert_exact("lazy faults for sequential 2MB access", lazy_faults, expected_pages);
    printf("  PASS: Sequential page touches faulted all 512 pages\n");

    lazy_free(request_size);
}

void
test_lazy_random_access(void)
{
    printf("[TEST] lazy_random_access\n");

    struct memstat before, after;
    int request_size = 1024 * 1024;
    int indices[] = {0, 10, 50, 100, 200, 255, 128, 64, 32, 16};
    int num_access = sizeof(indices) / sizeof(indices[0]);

    read_memstat(&before);
    char *addr = lazy_alloc(request_size);
    printf("  OK: sbrklazy 1MB = %p\n", addr);

    read_memstat(&after);
    assert_lazy_request_is_deferred(&before, &after);
    printf("  OK: request reserved virtual space without physical allocation\n");

    read_memstat(&before);
    for(int i = 0; i < num_access; i++) {
        int idx = indices[i];
        addr[idx * PAGE_SIZE] = 'D' + (idx % 26);
    }
    read_memstat(&after);

    int lazy_faults = lazy_fault_delta(&before, &after);
    int pages_used = free_page_delta(&before, &after);
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_faults, pages_used);

    for(int i = 0; i < num_access; i++) {
        int idx = indices[i];
        if(addr[idx * PAGE_SIZE] != 'D' + (idx % 26)) {
            printf("  FAIL: data error at page %d\n", idx);
            lazy_free(request_size);
            exit(1);
        }
    }

    assert_exact("pages allocated for random accesses", pages_used, num_access);
    assert_exact("lazy faults for random accesses", lazy_faults, num_access);
    printf("  PASS: Random page touches faulted exactly the touched pages\n");

    lazy_free(request_size);
}

void
test_lazy_multiple_allocs(void)
{
    printf("[TEST] lazy_multiple_allocs\n");

    struct memstat before, after;
    char *addrs[3];
    int request_size = 256 * 1024;

    read_memstat(&before);
    for(int i = 0; i < 3; i++) {
        addrs[i] = lazy_alloc(request_size);
        printf("  OK: reserved block %d at %p\n", i, addrs[i]);
    }
    read_memstat(&after);

    assert_lazy_request_is_deferred(&before, &after);
    printf("  OK: all three requests were deferred\n");

    read_memstat(&before);
    for(int i = 0; i < 3; i++) {
        addrs[i][0] = 'E' + i;
        addrs[i][PAGE_SIZE] = 'F' + i;
    }
    read_memstat(&after);

    int lazy_faults = lazy_fault_delta(&before, &after);
    int pages_used = free_page_delta(&before, &after);
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_faults, pages_used);

    for(int i = 0; i < 3; i++) {
        if(addrs[i][0] != 'E' + i || addrs[i][PAGE_SIZE] != 'F' + i) {
            printf("  FAIL: data error in block %d\n", i);
            for(int j = i; j >= 0; j--)
                lazy_free(request_size);
            exit(1);
        }
    }

    assert_exact("pages allocated for three two-page writes", pages_used, 6);
    assert_exact("lazy faults for three two-page writes", lazy_faults, 6);
    printf("  PASS: Each touched page was allocated exactly once\n");

    for(int i = 0; i < 3; i++)
        lazy_free(request_size);
}

void
test_lazy_zero_alloc(void)
{
    printf("[TEST] lazy_zero_alloc\n");

    struct memstat before, after;
    read_memstat(&before);
    char *addr = sbrklazy(0);
    read_memstat(&after);

    if(addr == (char*)-1) {
        printf("  FAIL: sbrklazy(0) returned -1\n");
        exit(1);
    }

    printf("  OK: sbrklazy(0) returned current break %p\n", addr);
    assert_exact("pages allocated by zero-size lazy request", free_page_delta(&before, &after), 0);
    assert_exact("lazy faults by zero-size lazy request", lazy_fault_delta(&before, &after), 0);
}

void
test_lazy_negative_alloc(void)
{
    printf("[TEST] lazy_negative_alloc\n");

    struct memstat before, after;
    char *base = lazy_alloc(2 * PAGE_SIZE);
    printf("  OK: reserved 2 lazy pages at %p\n", base);

    read_memstat(&before);
    char *old_break = sbrklazy(0);
    char *shrink_ret = sbrklazy(-PAGE_SIZE);
    read_memstat(&after);

    if(shrink_ret == (char*)-1) {
        printf("  FAIL: sbrklazy(-PAGE_SIZE) failed\n");
        lazy_free(PAGE_SIZE);
        exit(1);
    }
    if(shrink_ret != old_break) {
        printf("  FAIL: shrink returned %p, expected previous break %p\n", shrink_ret, old_break);
        lazy_free(PAGE_SIZE);
        exit(1);
    }

    assert_exact("pages allocated by shrinking untouched lazy memory", free_page_delta(&before, &after), 0);
    assert_exact("lazy faults during shrink", lazy_fault_delta(&before, &after), 0);

    if(sbrklazy(0) != old_break - PAGE_SIZE) {
        printf("  FAIL: heap break not reduced by one page\n");
        lazy_free(PAGE_SIZE);
        exit(1);
    }

    printf("  PASS: shrinking untouched lazy heap updates break without extra allocation\n");
    lazy_free(PAGE_SIZE);
}

void
test_lazy_very_large_alloc(void)
{
    printf("[TEST] lazy_very_large_alloc\n");

    struct memstat before, after;
    int request_size = 2 * 1024 * 1024;

    read_memstat(&before);
    char *addr = lazy_alloc(request_size);
    printf("  OK: sbrklazy 2MB = %p\n", addr);

    read_memstat(&after);
    assert_lazy_request_is_deferred(&before, &after);
    printf("  OK: request reserved virtual space without physical allocation\n");

    read_memstat(&before);
    addr[0] = 'G';
    addr[request_size - 1] = 'H';
    read_memstat(&after);

    int lazy_faults = lazy_fault_delta(&before, &after);
    int pages_used = free_page_delta(&before, &after);
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_faults, pages_used);

    if(addr[0] != 'G' || addr[request_size - 1] != 'H') {
        printf("  FAIL: boundary write verification failed\n");
        lazy_free(request_size);
        exit(1);
    }

    assert_exact("pages allocated for first/last byte access", pages_used, 2);
    assert_exact("lazy faults for first/last byte access", lazy_faults, 2);
    printf("  PASS: Very large lazy region only materialized touched boundary pages\n");

    lazy_free(request_size);
}

void
test_lazy_grow_sh_shrink(void)
{
    printf("[TEST] lazy_grow_sh_shrink\n");

    struct memstat before, after;
    int half_size = 512 * 1024;

    read_memstat(&before);
    char *addr1 = lazy_alloc(half_size);
    printf("  OK: sbrklazy 512KB = %p\n", addr1);
    read_memstat(&after);
    assert_lazy_request_is_deferred(&before, &after);
    printf("  OK: first grow was deferred\n");

    before = after;
    char *addr2 = lazy_alloc(half_size);
    printf("  OK: sbrklazy another 512KB = %p\n", addr2);
    read_memstat(&after);
    assert_lazy_request_is_deferred(&before, &after);
    printf("  OK: second grow was deferred\n");

    read_memstat(&before);
    addr1[0] = 'H';
    addr2[0] = 'I';
    read_memstat(&after);
    assert_exact("pages allocated after touching both regions", free_page_delta(&before, &after), 2);
    assert_exact("lazy faults after touching both regions", lazy_fault_delta(&before, &after), 2);

    read_memstat(&before);
    if(sbrk(-half_size) == (char*)-1) {
        printf("  FAIL: sbrk shrink failed\n");
        lazy_free(half_size * 2);
        exit(1);
    }
    read_memstat(&after);

    int pages_freed = (int)after.free_pages - (int)before.free_pages;
    printf("  RESULT: pages_freed=%d\n", pages_freed);

    assert_exact("pages reclaimed after shrinking touched upper half", pages_freed, 1);
    if(addr1[0] != 'H') {
        printf("  FAIL: lower half data corrupted after upper-half shrink\n");
        lazy_free(half_size);
        exit(1);
    }

    printf("  PASS: shrinking reclaimed only the touched page in the removed half\n");
    lazy_free(half_size);
}

void
test_lazy_sparse_array(void)
{
    printf("[TEST] lazy_sparse_array\n");

    struct memstat before, after;
    int request_size = 1024 * 1024;
    int accessed = 0;

    read_memstat(&before);
    char *addr = lazy_alloc(request_size);
    printf("  OK: sbrklazy 1MB = %p\n", addr);

    read_memstat(&after);
    assert_lazy_request_is_deferred(&before, &after);
    printf("  OK: request reserved virtual space without physical allocation\n");

    read_memstat(&before);
    for(int i = 0; i < 256; i += 10) {
        addr[i * PAGE_SIZE] = 'I' + (i % 26);
        accessed++;
    }
    read_memstat(&after);

    int lazy_faults = lazy_fault_delta(&before, &after);
    int pages_used = free_page_delta(&before, &after);
    printf("  RESULT: lazy_allocs=%d, pages_allocated=%d\n", lazy_faults, pages_used);

    for(int i = 0; i < 256; i += 10) {
        if(addr[i * PAGE_SIZE] != 'I' + (i % 26)) {
            printf("  FAIL: sparse data error at page %d\n", i);
            lazy_free(request_size);
            exit(1);
        }
    }

    assert_exact("pages allocated for sparse accesses", pages_used, accessed);
    assert_exact("lazy faults for sparse accesses", lazy_faults, accessed);
    printf("  PASS: Sparse accesses faulted exactly the touched pages\n");

    lazy_free(request_size);
}
