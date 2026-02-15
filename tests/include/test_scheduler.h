// test_scheduler.h - Scheduler algorithm unit test declarations
#ifndef TEST_SCHEDULER_H
#define TEST_SCHEDULER_H

// FCFS tests
void test_sched_fcfs_order(void);

// PRIORITY tests
void test_sched_priority_order(void);
void test_sched_priority_fcfs(void);

// SML tests
void test_sched_sml_queues(void);

// LOTTERY tests
void test_sched_lottery_basic(void);

// General tests
void test_sched_switch_preserves(void);
void test_sched_default_rr(void);
void test_sched_getscheduler(void);
void test_sched_setpriority(void);
void test_sched_stats_tracking(void);

#endif
