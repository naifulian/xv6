// test_mmap.h - mmap unit test declarations
#ifndef TEST_MMAP_H
#define TEST_MMAP_H

#include "test_common.h"

// mmap test functions (anonymous mapping only)
void test_mmap_basic(void);
void test_mmap_readonly(void);
void test_mmap_large(void);
void test_mmap_offset(void);
void test_mmap_multiple(void);
void test_mmap_munmap_null(void);

#endif // TEST_MMAP_H
