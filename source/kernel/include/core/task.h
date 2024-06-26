#ifndef TASK_H
#define TASK_H

#include "cpu/cpu.h"
#include "tools/list.h"

#define TASK_NAME_SIZE 32

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
    list_node_t all_node;
    list_node_t ready_node;
}task_t;

typedef struct {
    task_t *curr_task; 
    list_t ready_list; // list with tasks that are ready to run, including the running task (curr_task)
    list_t task_list; // list with all tasks
    task_t main_task;
}task_manager_t;

void task_init(task_t *task, const char *name, uint32_t entry, uint32_t esp);
void task_switch_from_to(task_t *from, task_t *to);
void task_manager_init(void);
void main_task_init(void);
task_t *task_main_task(void);
void task_set_ready(task_t *task);
void task_set_unready(task_t *task);
void sys_sched_yield();
void task_dispatch(void);
task_t *task_next_run(void);

#endif