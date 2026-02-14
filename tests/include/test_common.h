// test_common.h - Common test framework definitions
#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Test result macros
#define TEST_PASS()     printf("  OK: test passed\n")
#define TEST_FAIL(msg)  printf("  FAIL: %s\n", msg), exit(1)
#define TEST_INFO(msg)  printf("  INFO: %s\n", msg)

// Test assertion macros
#define ASSERT_NOT_NULL(ptr) \
    do { if((ptr) == 0) TEST_FAIL(#ptr " is NULL"); } while(0)

#define ASSERT_NULL(ptr) \
    do { if((ptr) != 0) TEST_FAIL(#ptr " is not NULL"); } while(0)

#define ASSERT_EQ(a, b, msg) \
    do { if((a) != (b)) { printf("  FAIL: %s (got %d, expected %d)\n", msg, (a), (b)); exit(1); } } while(0)

#define ASSERT_NE(a, b, msg) \
    do { if((a) == (b)) { printf("  FAIL: %s (both %d)\n", msg, (a)); exit(1); } } while(0)

// Wait status macros (for wait() system call)
#define WIFEXITED(status)  (((status) & 0x7f) == 0)
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define WIFSIGNALED(status) ((((status) & 0x7f) + 1) > 0 && (((status) & 0x7f) + 1) < 0x7f)
#define WTERMSIG(status) ((status) & 0x7f)

// Test case structure
struct test_case {
    char *name;
    char *description;
    void (*func)(void);
};

// Common test utilities
void print_test_header(const char *test_name);
void print_test_footer(const char *test_name);

#endif // TEST_COMMON_H
