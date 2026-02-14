// test_buddy.h - Buddy system unit test declarations
#ifndef TEST_BUDDY_H
#define TEST_BUDDY_H

#include "test_common.h"

// Buddy system test functions
void test_buddy_alloc_single_page(void);
void test_buddy_alloc_multiple_same(void);
void test_buddy_alloc_different_sizes(void);
void test_buddy_data_integrity(void);
void test_buddy_merge_pattern(void);
void test_buddy_fragmentation(void);
void test_buddy_stress_small(void);
void test_buddy_zero_size(void);
void test_buddy_large_allocation(void);
void test_buddy_free_null(void);
void test_buddy_double_free(void);

#endif // TEST_BUDDY_H
