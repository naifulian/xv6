// test_boundary.h - Boundary test declarations
#ifndef TEST_BOUNDARY_H
#define TEST_BOUNDARY_H

#include "test_common.h"

// Boundary test functions
void test_nproc_limit(void);
void test_nproc_exact(void);
void test_nproc_fork_bomb(void);
void test_nproc_wait_refork(void);
void test_nproc_getpid(void);
void test_nproc_orphaned(void);

#endif // TEST_BOUNDARY_H
