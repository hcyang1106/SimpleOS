#include "core/task.h"
#include "tools/klib.h"
#include "os_cfg.h"
#include "cpu/cpu.h"
#include "tools/log.h"
#include "comm/cpu_instr.h"
#include "tools/list.h"
#include "cpu/cpu.h"
#include "core/memory.h"
#include "cpu/mmu.h"

static task_manager_t task_manager;
static uint32_t idle_task_stack[1024];

void main_task_entry(int, int); // to test whether arguments matter

static void idle_task_entry(void) {
    for (;;) {
        hlt();
    }
}

void simple_switch(uint32_t **from , uint32_t *to);

static void tss_init(task_t *task, int flag, uint32_t entry, uint32_t esp) {
    // a process can be created a long time after the kernel runs
    // so we're unsure where is empty in gdt, so this alloc func is needed
    int tss_sel = gdt_alloc_desc(); 
    if (tss_sel <= 0) {
        log_printf("alloc for tss in gdt failed");
        goto tss_init_failed;
    }

    segment_desc_set(tss_sel, (uint32_t)&task->tss, sizeof(tss_t), SEG_P_PRESENT | SEG_DPL0 | SEG_TSS);

    kernel_memset(&task->tss, 0, sizeof(tss_t));

    // one page for stack level 0
    // stack level 3 is right behind the code of task
    uint32_t kernel_stack = mem_alloc_page(STACK_ZERO_PAGE_COUNT);
    if (kernel_stack == 0) {
        // has to free up the resources before mem_alloc_page!
        log_printf("mem alloc page for level zero stack failed");
        goto tss_init_failed;
    }

    int code_sel, data_sel;
    if (flag == TASK_FLAG_SYSTEM) {
        code_sel = KERNEL_SELECTOR_CS;
        data_sel = KERNEL_SELECTOR_DS;
    } else {
        code_sel = task_manager.task_code_sel | SEG_CPL3;
        data_sel = task_manager.task_data_sel | SEG_CPL3;
    }
    
    task->tss.eip = entry;
    task->tss.esp = esp;
    task->tss.esp0 = kernel_stack + STACK_ZERO_PAGE_COUNT * MEM_PAGE_SIZE;
    task->tss.ss0 = KERNEL_SELECTOR_DS;
    task->tss.ss = data_sel;
    task->tss.ds = task->tss.es = task->tss.fs = task->tss.gs = data_sel;
    task->tss.cs = code_sel;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF; // set if flag as 1
    uint32_t page_dir = memory_create_uvm();
    if (page_dir == 0) {
        log_printf("create page dir for process failed");
        goto tss_init_failed;
    }
    
    task->tss.cr3 = page_dir;
    task->tss_sel = tss_sel;
    return;

tss_init_failed:
    gdt_free_sel(tss_sel);
    if (kernel_stack) {
        mem_free_page((uint32_t)kernel_stack, STACK_ZERO_PAGE_COUNT);
    }
    return;
}

// cr3 is not replaced when doing task switching
void task_init(task_t *task, const char *name, int flag, uint32_t entry, uint32_t esp) {
    // recall the definition of null pointer
    // null pointer is unequal to any pointer pointing to an object or function
    ASSERT(task != (task_t*)0);
    tss_init(task, flag, entry, esp); // this is for hardware switching

    kernel_strncpy(task->name, name, TASK_NAME_SIZE);
    task->state = TASK_CREATED;
    task->curr_tick = TASK_TIME_TICKS_DEFAULT;
    task->sleep_tick = 0;

    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    list_node_init(&task->wait_node);

    irq_state_t state = irq_enter_protection();

    list_insert_last(&task_manager.task_list, &task->all_node);
    task_set_ready(task);
    
    irq_leave_protection(state);

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
    int sel = gdt_alloc_desc();
    segment_desc_set(sel, 0, 0xFFFFFFFF,
        SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL | SEG_TYPE_CODE | SEG_TYPE_RW | SEG_D
    );
    task_manager.task_code_sel = sel;

    sel = gdt_alloc_desc();
    segment_desc_set(sel, 0, 0xFFFFFFFF,
        SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL | SEG_TYPE_DATA | SEG_TYPE_RW | SEG_D
    );
    task_manager.task_data_sel = sel;

    task_manager.curr_task = (task_t*)0;
    list_init(&task_manager.ready_list);
    list_init(&task_manager.task_list);
    list_init(&task_manager.sleep_list);
    // stack grows from high to low and esp moves downwards first before pushing, so set as 1024 instead of 1023 
    task_init(&task_manager.idle_task, "idle task", TASK_FLAG_SYSTEM, (uint32_t)idle_task_entry, (uint32_t)&idle_task_stack[1024]);
}

void main_task_init(void) {
    extern uint8_t s_main_task, e_main_task; // physical address

    uint32_t copy_size = (uint32_t)&e_main_task - (uint32_t)&s_main_task;
    // assume that 10 pages are enough for main_task
    uint32_t page_count = MAIN_TASK_PAGE;
    
    // at first I wrote (uint32_t)&s_main_task + copy_size, this is wrong
    task_init(&task_manager.main_task, "main task", TASK_FLAG_NORMAL,
             (uint32_t)main_task_entry, (uint32_t)main_task_entry + copy_size);

    write_tr(task_manager.main_task.tss_sel);
    task_manager.curr_task = &task_manager.main_task;

    // has already mapped kernel code to virtual memory space
    // so can still execute kernel code
    // it seems that removing this is ok (before the need of allocating mem for task)
    // to my understanding, we can set any page dir as cr3 since 
    // we also map kernel to task's virtual mem space
    uint32_t page_dir = task_manager.main_task.tss.cr3;
    mmu_set_page_dir(page_dir); 
    
    // we want to paste it soe definitely with PTE_W
    alloc_mem_for_task(page_dir, page_count, (uint32_t)main_task_entry, PTE_P | PTE_W | PTE_U);
    kernel_memcpy(main_task_entry, &s_main_task, copy_size);
}

task_t *task_main_task(void) {
    return &task_manager.main_task;
}

void task_set_ready(task_t *task) {
    if (task == &task_manager.idle_task) {
        return;
    }
    task->state = TASK_READY;
    list_insert_last(&task_manager.ready_list, &task->run_node);
}

void task_set_unready(task_t *task) {
    if (task == &task_manager.idle_task) {
        return;
    }
    list_remove_node(&task_manager.ready_list, &task->run_node);
}

// give up using cpu but insert into ready list right away
void sys_sched_yield(void) {
    irq_state_t state = irq_enter_protection();

    // only the current task is in ready list
    if (list_count(&task_manager.ready_list) == 1) { 
        return;
    }
    task_set_unready(task_manager.curr_task);
    task_set_ready(task_manager.curr_task);
    task_dispatch();

    irq_leave_protection(state);
}

// select the next task and do switching
void task_dispatch(void) {
    // this function will not only exist in sys_sched_yield
    // it may be called independently, so we need protection
    irq_state_t state = irq_enter_protection();

    task_t *to = task_next_run();
    // if selected task equals the current task then no need to change
    if (to == task_manager.curr_task) {
        return;
    }
    task_t* from = task_manager.curr_task;
    task_manager.curr_task = to;
    to->state = TASK_RUNNING;
    task_switch_from_to(from, to);

    irq_leave_protection(state);
}

// returns the first task in list
// if no task return idle_task
task_t *task_next_run(void) {
    list_node_t* first = list_first(&task_manager.ready_list);
    if (!first) {
        return &task_manager.idle_task;
    }
    return parent_pointer(task_t, run_node, first);
}

task_t *task_current(void) {
    return task_manager.curr_task;
}

void task_time_tick(void) {
    task_t *curr_task = task_current();
    if (--curr_task->curr_tick == 0) {
        curr_task->curr_tick = TASK_TIME_TICKS_DEFAULT; // reset time
        if (list_count(&task_manager.ready_list) > 1) {
            task_set_unready(task_manager.curr_task);
            task_set_ready(task_manager.curr_task);
            task_dispatch();
        }
    }

    list_node_t *it = list_first(&task_manager.sleep_list);
    while (it) {
        list_node_t *next = list_node_next(it);
        task_t *pit = parent_pointer(task_t, run_node, it);
        if (--pit->sleep_tick == 0) {
            task_set_unsleep(pit);
            task_set_ready(pit);
        }
        it = next;
    }

    task_dispatch();
}

void sys_sleep(uint32_t ms) {
    irq_state_t state = irq_enter_protection();

    int ticks = (ms + (OS_TICK_MS - 1)) / OS_TICK_MS;
    task_set_unready(task_manager.curr_task);
    task_set_sleep(task_manager.curr_task, ticks);
    task_dispatch();

    irq_leave_protection(state);
}

void task_set_sleep(task_t *task, int ticks) {
    if (ticks == 0) {
        return;
    }
    task->state = TASK_SLEEP;
    task->sleep_tick = ticks;
    list_insert_last(&task_manager.sleep_list, &task->run_node);
}

// when doing unready/unsleep, there will be a set_xxx following
// so state is not set in unready/unsleep
void task_set_unsleep(task_t *task) {
    list_remove_node(&task_manager.sleep_list, &task->run_node);
}

// changes //
// 1. main task and init task first locate in kernel code
// after page mode is turned on, tss needs to save the info of cr3 (page dir address)
// so we need to setup page tables for the created tasks (memory_create_uvm)
// how to setup page tables? 
// (from kernel's perspective, we set memory below 1MB for kernel [currently, may change afterwards])
// * we allocate extra memory starting from 1MB
// * we allocate page tables for task starting from 1MB
// * process also needs to access kernel code, so we make address below 0x80000000 map to kernel code (below 1MB in physical mem)
// * (above 0x80000000 for task)

// 2. make task separate from kernel code
// create main_task.c and main_task_entry.S

// 3. modify linker script to make main_task_entry at virtual address 0x80000000 
// main_task's physical address is still within kernel

// 4. copy main_task code from kernel to 0x80000000
// why copy instead of mapping?

// 5. make tasks unable to access kernel code, otherwise it would be dangerous
// since we're using flat model (set segment regs as 0 and seg size as 0xFFFFFFFF)
// which means we can't simply utilize segmentation to protect kernel
// so we use the U/S bit of pte/pde to preovide protection
// U/S bit = 0 => privilege level 0, 1, 2 can access, U/S bit = 1 => privilege 3 can access

// * page entry without write enable can still be written when privilege level is high (e.g. zero level)
// so it is used to control privilege level 3
// first set level 3 seg regs when initializing tasks (and of course setup gdt too)
// however, this result in main_task running in level 0 and "idle task" running in level 3
// we want main_task running in level 3 and idle task running in 0
// so first we make idle task initialization sepecialized

// set up different stack for different privilege level
// when exception happens (how about interrupt?), CPL changes to 0 when executing handler function
// i think it's because we set KERNEL_SELECTOR_CS in every gate entry
// also there is DPL in gate entry?
// why do we need different stack for different level?
// why it doesn't run when no level 0 stack is allocated?

// for different levels of the same task, they should have different stacks (one for 0, one for 3)