// test_cow.h - Copy-on-Write unit test declarations
#ifndef TEST_COW_H
#define TEST_COW_H

#include "test_common.h"

// COW test functions
void test_cow_fork_exec(void);
void test_cow_partial_write(void);
void test_cow_full_write(void);
void test_cow_multiple_forks(void);
void test_cow_fork_exec_chain(void);
void test_cow_memory_sharing(void);

#endif // TEST_COW_H
