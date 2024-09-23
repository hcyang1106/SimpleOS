#ifndef TASK_H
#define TASK_H

#include "cpu/cpu.h"
#include "tools/list.h"
#include "fs/file.h"

#define TASK_NAME_SIZE 32
#define TASK_TIME_TICKS_DEFAULT 10
#define MAIN_TASK_PAGE 10
#define TASK_FLAG_SYSTEM 1
#define TASK_FLAG_NORMAL 0
#define STACK_ZERO_PAGE_COUNT 1
#define OPEN_FILE_NUM 128

typedef struct _task_t {
    enum {
        TASK_CREATED,
        TASK_RUNNING,
        TASK_SLEEP,
        TASK_READY,
        TASK_WAITING,
        TASK_ZOMBIE,
    }state;
    int status;
    uint32_t pid;
    char name [TASK_NAME_SIZE];
    struct _task_t *parent; // should use struct _task because task_t is not seen yet
    // uint32_t *stack; for simple switch
    tss_t tss;   // used when using hardware to switch tasks
    int tss_sel; // used when using hardware to switch tasks
    int curr_tick; // curr_tick counts from 10 to 0, then reset to 10 again
    int sleep_tick;
    list_node_t all_node;
    // list_node_t wait_node; // for wait list
    list_node_t run_node; // this will move between ready/sleep list

    uint32_t heap_start;
    uint32_t heap_end;

    file_t *file_table[OPEN_FILE_NUM];
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

typedef struct _task_args_t {
    uint32_t ret_addr;
    uint32_t argc;
    char **argv;
}task_args_t;

int task_init(task_t *task, const char *name, int flag, uint32_t entry, uint32_t esp);
void task_switch_from_to(task_t *from, task_t *to);
void task_manager_init(void);
void main_task_init(void);
task_t *task_main_task(void);
void task_set_ready(task_t *task);
void task_set_unready(task_t *task);
void sys_yield();
void task_dispatch(void);
task_t *task_next_run(void);
task_t *task_current(void);
void task_time_tick(void);
void task_set_sleep(task_t *task, int ticks);
void task_set_unsleep(task_t *task);
void sys_msleep(uint32_t ms);
uint32_t sys_getpid(void);
int sys_fork(void);
void sys_print_msg(const char *fmt, int arg);
int sys_execve(char *name, char **argv, char **env);
void sys_exit(int status);
int sys_wait(int *status);

file_t *task_file(int fd);
int task_alloc_fd(file_t *file);
void task_remove_fd(int fd);

#endif