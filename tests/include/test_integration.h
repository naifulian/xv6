// test_integration.h - Integration test declarations
#ifndef TEST_INTEGRATION_H
#define TEST_INTEGRATION_H

#include "test_common.h"

// fork+exec integration tests
void test_fork_exec_single(void);
void test_fork_exec_multiple(void);
void test_fork_exec_with_data(void);
void test_fork_exec_chain(void);
void test_fork_exec_exit_status(void);

// mmap+fork integration tests
void test_mmap_fork_private(void);
void test_mmap_fork_shared(void);
void test_mmap_fork_multiple(void);
void test_mmap_fork_exec(void);
void test_mmap_fork_anonymous(void);

#endif // TEST_INTEGRATION_H
