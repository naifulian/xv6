// System Monitor - Real-time system monitoring tool
// Similar to Linux 'top' command
// Displays CPU, memory, process, and scheduler statistics

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

struct sys_snapshot {
  uint64 timestamp;
  
  int cpu_usage;
  uint64 context_switches;
  uint64 total_ticks;
  
  int total_pages;
  int free_pages;
  
  int nr_running;
  int nr_sleeping;
  int nr_zombie;
  int nr_total;
  
  int sched_policy;
  int runqueue_len;
};

const char*
scheduler_name(int policy)
{
    switch(policy) {
        case 0: return "DEFAULT (Round-Robin)";
        case 1: return "FCFS";
        case 2: return "PRIORITY";
        case 3: return "SML";
        case 4: return "LOTTERY";
        case 5: return "SJF";
        case 6: return "SRTF";
        case 7: return "MLFQ";
        case 8: return "CFS";
        default: return "UNKNOWN";
    }
}

void
print_bar(const char *label, int value, int max_value, int width)
{
    printf("%s [", label);
    int filled = (value * width) / max_value;
    for(int i = 0; i < width; i++) {
        if(i < filled) {
            printf("#");
        } else {
            printf(" ");
        }
    }
    printf("] %d%%\n", (value * 100) / max_value);
}

void
display_snapshot(struct sys_snapshot *snap)
{
    printf("\033[2J\033[H");
    
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║              XV6 System Monitor (sysmon)                 ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("┌─ CPU Statistics ─────────────────────────────────────────┐\n");
    printf("│ Timestamp: %-10d ticks                              │\n", (int)snap->timestamp);
    print_bar("│ CPU Usage ", snap->cpu_usage, 100, 30);
    printf("│ Context Switches: %-10d                             │\n", (int)snap->context_switches);
    printf("│ Total Ticks: %-10d                                  │\n", (int)snap->total_ticks);
    printf("└──────────────────────────────────────────────────────────┘\n");
    printf("\n");
    
    printf("┌─ Memory Statistics ───────────────────────────────────────┐\n");
    int mem_usage = ((snap->total_pages - snap->free_pages) * 100) / snap->total_pages;
    print_bar("│ Memory    ", mem_usage, 100, 30);
    printf("│ Free: %d / %d pages (%d%% free)                │\n", 
           snap->free_pages, snap->total_pages, 
           (snap->free_pages * 100) / snap->total_pages);
    printf("└──────────────────────────────────────────────────────────┘\n");
    printf("\n");
    
    printf("┌─ Process Statistics ──────────────────────────────────────┐\n");
    printf("│ Total Processes: %-10d                            │\n", snap->nr_total);
    printf("│ Running: %-5d  Sleeping: %-5d  Zombie: %-5d        │\n", 
           snap->nr_running, snap->nr_sleeping, snap->nr_zombie);
    printf("│ Runqueue Length: %-10d                             │\n", snap->runqueue_len);
    printf("└──────────────────────────────────────────────────────────┘\n");
    printf("\n");
    
    printf("┌─ Scheduler Information ───────────────────────────────────┐\n");
    printf("│ Current Policy: %s\n", scheduler_name(snap->sched_policy));
    printf("│ Policy ID: %-10d                                  │\n", snap->sched_policy);
    printf("└──────────────────────────────────────────────────────────┘\n");
    printf("\n");
    
    printf("Press Ctrl+C to exit\n");
}

int
main(int argc, char *argv[])
{
    int interval = 1;
    
    if(argc > 1) {
        interval = atoi(argv[1]);
        if(interval < 1) {
            interval = 1;
        }
    }
    
    printf("Starting system monitor (refresh interval: %d second)...\n", interval);
    printf("Press Ctrl+C to exit\n\n");
    
    struct sys_snapshot snap;
    
    while(1) {
        if(getsnapshot(&snap) < 0) {
            printf("Error: failed to get system snapshot\n");
            exit(1);
        }
        
        display_snapshot(&snap);
        
        sleep(interval);
    }
    
    exit(0);
}
