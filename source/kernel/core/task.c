#include "core/task.h"
#include "tools/klib.h"
#include "os_cfg.h"
#include "cpu/cpu.h"
#include "tools/log.h"
#include "comm/cpu_instr.h"
#include "tools/list.h"

static task_manager_t task_manager;

void simple_switch(uint32_t **from , uint32_t *to);

static void tss_init(task_t *task, uint32_t entry, uint32_t esp) {
    // a process can be created a long time after the kernel runs
    // so we're unsure where is empty in gdt, so this alloc func is needed
    int tss_sel = gdt_alloc_desc(); 
    if (tss_sel < 0) {
        log_printf("alloc for tss in gdt failed");
        return;
    }

    segment_desc_set(tss_sel, (uint32_t)&task->tss, sizeof(tss_t), SEG_P_PRESENT | SEG_DPL0 | SEG_TSS);

    kernel_memset(&task->tss, 0, sizeof(tss_t));
    task->tss.eip = entry;
    task->tss.esp = task->tss.esp0 = esp;
    task->tss.ss = task->tss.ss0 = KERNEL_SELECTOR_DS;
    task->tss.ds = task->tss.es = task->tss.fs = task->tss.gs = KERNEL_SELECTOR_DS;
    task->tss.cs = KERNEL_SELECTOR_CS;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF; // set if flag as 1

    task->tss_sel = tss_sel;
}

void task_init(task_t *task, const char *name, uint32_t entry, uint32_t esp) {
    // recall the definition of null pointer
    // null pointer is unequal to any pointer pointing to an object or function
    ASSERT(task != (task_t*)0);
    tss_init(task, entry, esp); // this is for hardware switching

    kernel_strncpy(task->name, name, TASK_NAME_SIZE);
    task->state = TASK_CREATED;
    list_node_init(&task->all_node);
    list_node_init(&task->ready_node);
    list_insert_last(&task_manager.task_list, &task->all_node);

    task_set_ready(task);
    
    // simple task switching
    // uint32_t *p_esp = (uint32_t*)esp;
    // if (p_esp) {
    //     // this doen't work because it is rvalue after type conversion
    //     // *(--((uint32_t*)esp)) = entry;
    //     *(--p_esp) = entry;
    //     *(--p_esp) = 0;
    //     *(--p_esp) = 0;
    //     *(--p_esp) = 0;
    //     *(--p_esp) = 0;
    //     task->stack = p_esp;
    // }
}

void task_switch_from_to(task_t *from, task_t *to) {
    switch_to_tss(to->tss_sel); // this is for hardware switching
    // simple_switch(&from->stack, to->stack);
}

void task_manager_init(void) {
    task_manager.curr_task = (task_t*)0;
    list_init(&task_manager.ready_list);
    list_init(&task_manager.task_list);
}

void main_task_init(void) {
    task_init(&task_manager.main_task, "main task", 0, 0);
    write_tr(task_manager.main_task.tss_sel);
    task_manager.curr_task = &task_manager.main_task;
}

task_t *task_main_task(void) {
    return &task_manager.main_task;
}

void task_set_ready(task_t *task) {
    task->state = TASK_READY;
    list_insert_last(&task_manager.ready_list, &task->ready_node);
}

void task_set_unready(task_t *task) {
    list_remove_node(&task_manager.ready_list, &task->ready_node);
}

// give up using cpu but insert into ready list right away
void sys_sched_yield(void) {
    // only the current task is in ready list
    if (list_count(&task_manager.ready_list) == 1) { 
        return;
    }
    task_set_unready(task_manager.curr_task);
    task_set_ready(task_manager.curr_task);
    task_dispatch();
}

void task_dispatch(void) {
    task_t *to = task_next_run();
    // if selected task equals the current task then no need to change
    if (to == task_manager.curr_task) {
        return;
    }
    task_t* from = task_manager.curr_task;
    task_manager.curr_task = to;
    to->state = TASK_RUNNING;
    task_switch_from_to(from, to);
}

// returns the first task in list
task_t *task_next_run(void) {
    list_node_t* first = list_first(&task_manager.ready_list);
    return parent_pointer(task_t, ready_node, first);
}