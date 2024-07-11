#ifndef TASK_H
#define TASK_H

#include "cpu/cpu.h"
#include "tools/list.h"

#define TASK_NAME_SIZE 32
#define TASK_TIME_TICKS_DEFAULT 10
#define MAIN_TASK_PAGE 10
#define TASK_FLAG_SYSTEM 1
#define TASK_FLAG_NORMAL 0
#define STACK_ZERO_PAGE_COUNT 1

typedef struct _task_t {
    enum {
        TASK_CREATED,
        TASK_RUNNING,
        TASK_SLEEP,
        TASK_READY,
        TASK_WAITING,
    }state;
    char name [TASK_NAME_SIZE];
    uint32_t *stack;
    tss_t tss;   // used when using hardware to switch tasks
    int tss_sel; // used when using hardware to switch tasks
    int curr_tick; // curr_tick counts from 10 to 0, then reset as 10 again
    int sleep_tick;
    list_node_t all_node;
    list_node_t wait_node; // for wait list
    list_node_t run_node; // this will move between ready/sleep list
}task_t;

typedef struct {
    task_t *curr_task; 
    list_t ready_list; // list with tasks that are ready to run, including the running task (curr_task)
    list_t task_list; // list with all tasks
    list_t sleep_list;
    task_t main_task;
    task_t idle_task;
    int task_code_sel;
    int task_data_sel;
}task_manager_t;

void task_init(task_t *task, const char *name, int flag, uint32_t entry, uint32_t esp);
void task_switch_from_to(task_t *from, task_t *to);
void task_manager_init(void);
void main_task_init(void);
task_t *task_main_task(void);
void task_set_ready(task_t *task);
void task_set_unready(task_t *task);
void sys_sched_yield();
void task_dispatch(void);
task_t *task_next_run(void);
task_t *task_current(void);
void task_time_tick(void);
void task_set_sleep(task_t *task, int ticks);
void task_set_unsleep(task_t *task);
void sys_sleep(uint32_t ms);

#endif