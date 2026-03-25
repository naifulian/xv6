#ifndef _USER_H_
#define _USER_H_

#include "kernel/types.h"

#define SBRK_ERROR ((char *)-1)

struct stat;

// Process info structure for user space
struct pstat {
  int pid;
  int state;
  int priority;
  int tickets;
  int sched_class;
  uint sz;
  uint heap_end;
  int vma_count;
  int mmap_regions;
  uint mmap_bytes;
  int rutime;
  int retime;
  int stime;
  char name[16];
};

struct vma_info {
  int pid;
  int slot;
  uint64 start;
  uint64 end;
  uint64 length;
  int prot;
  int flags;
};

// Memory statistics structure for user space
struct memstat {
  uint total_pages;
  uint free_pages;
  uint buddy_merges;
  uint buddy_splits;
  uint cow_faults;
  uint lazy_allocs;
  uint cow_copy_pages;
};

// System snapshot structure for monitoring
struct sys_snapshot {
  uint64 timestamp;
  char sched_name[16];
  int sched_policy;
  int cpu_usage;
  uint64 context_switches;
  uint64 total_ticks;
  int total_pages;
  int free_pages;
  int nr_running;
  int nr_sleeping;
  int nr_zombie;
  int nr_total;
  int runqueue_len;
};

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sys_sbrk(int,int);
int pause(int);
int uptime(void);
int sleep(int);
int setscheduler(int);
int getscheduler(void);
void* mmap(void*, uint, uint, uint, uint, uint);
int munmap(void*, uint);
int getptable(struct pstat*, int);
int getmemstat(struct memstat*);
int setpriority(int, int);
int getstats(void);
int getsnapshot(void*);
int getprocvmas(int, struct vma_info*, int);
int telemetrywrite(const void*, int);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
char* sbrk(int);
char* sbrklazy(int);

// Memory info helpers (compatible with baseline)
uint memtotal(void);
uint memfree(void);

// printf.c
void fprintf(int, const char*, ...) __attribute__ ((format (printf, 2, 3)));
void printf(const char*, ...) __attribute__ ((format (printf, 1, 2)));
void telemetry_printf(const char*, ...) __attribute__ ((format (printf, 1, 2)));

// umalloc.c
void* malloc(uint);
void free(void*);

#endif /* _USER_H_ */
