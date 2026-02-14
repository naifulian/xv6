// test_lazy.h - Lazy allocation unit test declarations
#ifndef TEST_LAZY_H
#define TEST_LAZY_H

#include "test_common.h"

// Lazy allocation test functions
void test_lazy_large_alloc_small_access(void);
void test_lazy_large_alloc_full_access(void);
void test_lazy_sequential_access(void);
void test_lazy_random_access(void);
void test_lazy_multiple_allocs(void);
void test_lazy_zero_alloc(void);
void test_lazy_negative_alloc(void);
void test_lazy_very_large_alloc(void);
void test_lazy_grow_sh_shrink(void);
void test_lazy_sparse_array(void);

#endif // TEST_LAZY_H
