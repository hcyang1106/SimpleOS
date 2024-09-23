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
#include "core/syscall.h"
#include "comm/elf.h"
#include "fs/fs.h"

static task_manager_t task_manager;
static uint32_t idle_task_stack[1024];
static task_t task_table[TASK_NUM];
static mutex_t task_table_mutex;

void main_task_entry(int, int); // to test whether arguments matter

static void idle_task_entry(void) {
    int i = 0;
    for (;;) {
        i++;
        hlt();
    }
}

void simple_switch(uint32_t **from , uint32_t *to);

static int tss_init(task_t *task, int flag, uint32_t entry, uint32_t esp) {
    // a process can be created a long time after the kernel runs
    // so we're unsure where is empty in gdt, so this alloc func is needed
    int tss_sel = gdt_alloc_desc(); 
    if (tss_sel < 0) {
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

    return 0;

tss_init_failed:
    gdt_free_sel(tss_sel);
    if (kernel_stack) {
        mem_free_page((uint32_t)kernel_stack, STACK_ZERO_PAGE_COUNT);
    }
    return -1;
}

void tss_uninit(tss_t tss, int sel) {
    gdt_free_sel(sel);
    mem_free_page(tss.esp0 - MEM_PAGE_SIZE, 1); // why? because tss.esp never changes
    memory_destroy_uvm(tss.cr3);
}

// cr3 is not replaced when doing task switching
int task_init(task_t *task, const char *name, int flag, uint32_t entry, uint32_t esp) {
    // recall the definition of null pointer
    // null pointer is unequal to any pointer pointing to an object or function
    ASSERT(task != (task_t*)0);
    int ret = tss_init(task, flag, entry, esp); // this is for hardware switching
    if (ret < 0) {
        return -1;
    }

    task->pid = (uint32_t)task;
    kernel_strncpy(task->name, name, TASK_NAME_SIZE);
    task->parent = (task_t*)0;
    task->state = TASK_CREATED;
    task->status = 0;
    task->curr_tick = TASK_TIME_TICKS_DEFAULT;
    task->sleep_tick = 0;

    task->heap_start = 0;
    task->heap_end = 0;

    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    // list_node_init(&task->wait_node);

    kernel_memset(&task->file_table, 0, sizeof(task->file_table));

    irq_state_t state = irq_enter_protection();

    list_insert_last(&task_manager.task_list, &task->all_node);
    
    irq_leave_protection(state);

    return 0;

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

void task_start(task_t *task) {
    irq_state_t state = irq_enter_protection();
    task_set_ready(task);
    irq_leave_protection(state);
}

void task_uninit(task_t *task) {
    tss_uninit(task->tss, task->tss_sel);
    kernel_memset((void*)task, 0, sizeof(task_t));
}

void task_switch_from_to(task_t *from, task_t *to) {
    switch_to_tss(to->tss_sel); // this is for hardware switching
    // simple_switch(&from->stack, to->stack);
}

void task_manager_init(void) {
    kernel_memset(task_table, 0, sizeof(task_table));
    mutex_init(&task_table_mutex);

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
    task_start(&task_manager.idle_task);
}

void main_task_init(void) {
    extern uint8_t s_main_task, e_main_task; // physical address

    uint32_t copy_size = (uint32_t)&e_main_task - (uint32_t)&s_main_task;
    // assume that 10 pages are enough for main_task
    uint32_t page_count = MAIN_TASK_PAGE;
    
    // at first I wrote (uint32_t)&s_main_task + copy_size, this is wrong
    task_init(&task_manager.main_task, "main task", TASK_FLAG_NORMAL,
             (uint32_t)main_task_entry, (uint32_t)main_task_entry + page_count * MEM_PAGE_SIZE);

    task_manager.main_task.heap_start = (uint32_t)&e_main_task;
    task_manager.main_task.heap_end = (uint32_t)&e_main_task;

    write_tr(task_manager.main_task.tss_sel);
    task_manager.curr_task = &task_manager.main_task;

    // has already mapped kernel code to virtual memory space
    // so can still execute kernel code
    // it seems that removing this is ok (before the need of allocating mem for task)
    // to my understanding, we can set any page dir as cr3 since 
    // we also map kernel to task's virtual mem space
    uint32_t page_dir = task_manager.main_task.tss.cr3;
    mmu_set_page_dir(page_dir); 
    
    // we want to paste it so definitely with PTE_W
    alloc_mem_for_task(page_dir, page_count, (uint32_t)main_task_entry, PTE_P | PTE_W | PTE_U);
    kernel_memcpy(main_task_entry, &s_main_task, copy_size);

    task_start(&task_manager.main_task);
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
void sys_yield(void) {
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

void sys_exit(int status) {
    task_t *task = task_current();
    for (int fd = 0; fd < OPEN_FILE_NUM; fd++) {
        file_t *file = task->file_table[fd];
        if (file) {
            sys_close(fd);
            task->file_table[fd] = (file_t*)0;
        }
    }

    int set_ready_main = 0;
    mutex_lock(&task_table_mutex);
    for (int i = 0; i < OPEN_FILE_NUM; i++) {
        task_t *t = task_table + i;
        if (t->parent == task) {
            t->parent = &task_manager.main_task; // all child processes go under main_task
            if (t->state == TASK_ZOMBIE) {
                set_ready_main = 1; // main_task has to be woken up
            }
        }
    }
    mutex_unlock(&task_table_mutex);

    irq_state_t state = irq_enter_protection();

    // we wake it up here if main_task is different from parent
    // otherwise it they are the same and it will be waken up in the following code
    if (set_ready_main && task->parent != &task_manager.main_task && 
        task_manager.main_task.state == TASK_WAITING) {
        task_set_ready(&task_manager.main_task);
    }

    if (task->parent->state == TASK_WAITING) {
        task_set_ready(task->parent);
    }
    
    task->state = TASK_ZOMBIE;
    task->status = status;
    task_set_unready(task);
    task_dispatch();
    irq_leave_protection(state);
}

// wait until a child process has exit
int sys_wait(int *status) {
    task_t *curr_task = task_current();

    while (1) {
        mutex_lock(&task_table_mutex);
        for (int i = 0; i < TASK_NUM; i++) {
            task_t *task = task_table + i;
            if (task->parent != curr_task) {
                continue;
            }

            if (task->state == TASK_ZOMBIE) {
                int pid = task->pid;
                *status = task->status;
                // resource release
                memory_destroy_uvm(task->tss.cr3);
                mem_free_page(task->tss.esp0 - MEM_PAGE_SIZE, 1); // why?
                kernel_memset(task, 0, sizeof(task_t));
                mutex_unlock(&task_table_mutex);
                return pid;
            }
        }
        mutex_unlock(&task_table_mutex);

        irq_state_t state = irq_enter_protection();
        task_set_unready(curr_task);
        curr_task->state = TASK_WAITING;
        task_dispatch();
        irq_leave_protection(state);
    }
    
    return 0;
}

// select the next task and do switching
void task_dispatch(void) {
    // this function will not only exist in sys_yield
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

void sys_msleep(uint32_t ms) {
    irq_state_t state = irq_enter_protection();

    int ticks = (ms + (OS_TICK_MS - 1)) / OS_TICK_MS;
    task_set_unready(task_manager.curr_task);
    task_set_sleep(task_manager.curr_task, ticks);
    task_dispatch();

    irq_leave_protection(state);
}

uint32_t sys_getpid(void) {
    return task_current()->pid;
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

static task_t *alloc_task(void) {
    task_t *task = (task_t*)0;

    mutex_lock(&task_table_mutex);

    for (int i = 0; i < TASK_NUM; i++) {
        if (task_table[i].pid == 0) {
            task = &task_table[i];
            break;
        }
    }

    mutex_unlock(&task_table_mutex);

    return task;
}

static void free_task(task_t *task) {
    mutex_lock(&task_table_mutex);

    task->pid = 0;

    mutex_unlock(&task_table_mutex);
}

static void copy_opened_files(task_t *parent, task_t *child) {
    for (int i = 0; i < OPEN_FILE_NUM; i++) {
        file_t *file = *(parent->file_table + i);
        if (file) {
            *(child->file_table + i) = file;
            file_inc_ref(file);
        }
    }
}

// child process will start executing from the "next instruction of lcall"
// the return value of below function is for parent process
// child process will not go through this,
// it only starts from the instruction from "next instruction of lcall"
// so to return zero for child process, we modify eax
// child process starts from privilege level 3

// high level idea of fork: copy every registers and "create a new page table",
// making the same virt addr point to a copied phy mem
int sys_fork(void) {
    task_t *parent = task_current();
    task_t *child = alloc_task();
    if (!child) {
        goto fork_failed;
    }

    syscall_frame_t *frame = (syscall_frame_t*)(parent->tss.esp0 - sizeof(syscall_frame_t)); // why?

    int status = task_init(child, parent->name, 0, frame->eip,
                           frame->esp + sizeof(uint32_t)*SYSCALL_PARAM_COUNT); // clean up params pushed
    if (status < 0) {
        goto fork_failed;
    }

    copy_opened_files(parent, child);

    child->tss.cs = frame->cs;
    child->tss.ds = frame->ds;
    child->tss.es = frame->es;
    child->tss.fs = frame->fs;
    child->tss.gs = frame->gs;
    child->tss.eflags = frame->eflags;

    child->tss.ebx = frame->ebx;
    // child->tss.eax = frame->eax;
    child->tss.eax = 0;
    child->tss.ecx = frame->ecx;
    child->tss.edx = frame->edx;
    child->tss.esi = frame->esi;
    child->tss.edi = frame->edi;
    child->tss.ebp = frame->ebp;

    child->parent = parent;
    // should not use the same page table, 
    // otherwise two processes will modify the same stack
    // child->tss.cr3 = parent->tss.cr3;
    uint32_t cr3 = memory_copy_uvm(parent->tss.cr3);
    if (cr3 < 0) {
        goto fork_failed;
    } else {
        child->tss.cr3 = cr3;
    }

    task_start(child);
    
    return child->pid;

fork_failed:
    if (child) {
        free_task(child);
    }
    if (status) {
        task_uninit(child);
    }

    return -1;
}

static int load_phdr(int file, Elf32_Phdr* phdr, uint32_t page_dir) {
    int page_count = up(phdr->p_memsz, MEM_PAGE_SIZE) / MEM_PAGE_SIZE;
    int ret = alloc_mem_for_task(page_dir, page_count, phdr->p_vaddr, PTE_P | PTE_U | PTE_W);
    if (ret < 0) {
        log_printf("alloc mem for task failed");
        return -1;
    }

    // move the pos in file to the start of doing copy
    if (sys_lseek(file, phdr->p_offset, 0) < 0) {
        log_printf("lseek failed");
        return -1;
    }

    // target address, however it can only be used with new page dir
    // and we're currently using old page pir
    // vaddr 4096 aligned, if not then we cannot do the stuffs in while loop
    uint32_t vaddr = phdr->p_vaddr; 
    uint32_t size = phdr->p_filesz;

    // the physical pages may not be continuous
    while (size > 0) {
        int curr_size = (size > MEM_PAGE_SIZE) ? MEM_PAGE_SIZE : size;
        uint32_t paddr = memory_get_paddr(page_dir, vaddr);
        int ret = sys_read(file, (char*)paddr, curr_size);
        if (ret < 0) {
            log_printf("sys read failed");
            return -1;
        }
        
        size -= curr_size;
        vaddr += curr_size;
    }

    return 0;
}

static uint32_t load_elf_file(task_t *task, const char *name, uint32_t page_dir) {
    Elf32_Ehdr elf_hdr;
    Elf32_Phdr elf_phdr;

    // load from disk to mem (it is placed in kernel bss part)
    int file = sys_open(name, 0);
    if (file < 0) {
        log_printf("open %s failed", name);
        goto load_elf_failed;
    }

    // validate elf hdr
    int cnt = sys_read(file, (char*)&elf_hdr, sizeof(Elf32_Ehdr));
    if (cnt < sizeof(Elf32_Ehdr)){
        log_printf("read elf hdr error. size=%d", cnt);
        goto load_elf_failed;
    }

    // validate header
    if ((elf_hdr.e_ident[0] != 0x7f) || (elf_hdr.e_ident[1] != 'E')
        || (elf_hdr.e_ident[2] != 'L') || (elf_hdr.e_ident[3] != 'F')) {
        log_printf("elf ident validation failed");
        goto load_elf_failed; 
    }

    
    // elf file consists of program header 0, program header 1...
    // e_phoff is the starting position of program header 0 
    uint32_t e_phoff = elf_hdr.e_phoff;
    for (int i = 0; i < elf_hdr.e_phnum; i++, e_phoff += elf_hdr.e_phentsize) {
        if (sys_lseek(file, e_phoff, 0) < 0) {
            log_printf("sys_lseek failed");
            goto load_elf_failed;
        }

        cnt = sys_read(file, (char*)&elf_phdr, sizeof(elf_phdr));
        if (cnt < sizeof(elf_phdr)) {
            log_printf("sys_read failed");
            goto load_elf_failed;
        }

        if (elf_phdr.p_type != PT_LOAD || elf_phdr.p_vaddr < MEM_TASK_BASE) {
            continue;
        }

        int ret = load_phdr(file, &elf_phdr, page_dir);
        if (ret < 0) {
            log_printf("load program failed");
            goto load_elf_failed;
        }

        task->heap_start = elf_phdr.p_vaddr + elf_phdr.p_memsz;
        task->heap_end = task->heap_start;
    }

    sys_close(file);

    return elf_hdr.e_entry;

load_elf_failed:
    if (file) {
        sys_close(file);
    }
    return -1;
}

static int copy_args(char *to, uint32_t page_dir, int argc, char **argv) {
    task_args_t task_args;
    task_args.argc = argc;
    task_args.argv = (char**)(to + sizeof(task_args_t));
    // ret address is not needed because not returning
    
    // copy task_args part
    int ret = memory_copy_uvm_data((uint32_t)to, page_dir, (uint32_t)&task_args, sizeof(task_args_t));
    if (ret < 0) {
        return -1;
    }

    // args part
    char *arg_dest = to + sizeof(task_args_t) + sizeof(char*) * (argc + 1);
    // to is virt addr of new page dir
    // char **p_arg_dest = (char**)(to + sizeof(task_args_t));
    char **p_arg_dest = (char**)memory_get_paddr(page_dir, (uint32_t)(to + sizeof(task_args_t)));
    if (!p_arg_dest) {
        return -1;
    }

    for (int i = 0; i < argc; i++) {
        int len = kernel_strlen(argv[i]) + 1; // remember the null char
        int ret = memory_copy_uvm_data((uint32_t)arg_dest, page_dir, (uint32_t)argv[i], len);
        if (ret < 0) {
            return -1;
        }
        p_arg_dest[i] = arg_dest;
        arg_dest += len;
    }

    p_arg_dest[argc] = (char*)0;

    return 0;
}

// after exec the code and data will be completely replaced
// child of fork does not go through the sys_fork code
// execve does (it is changing itself)
int sys_execve(char *name, char **argv, char **env) {
    task_t *task = task_current();

    kernel_strncpy(task->name, get_file_name(name), TASK_NAME_SIZE);

    uint32_t old_page_dir = task->tss.cr3;

    uint32_t new_page_dir = memory_create_uvm();
    if (!new_page_dir) {
        goto exec_failed;
    }

    // this also sets up page tables
    uint32_t entry = load_elf_file(task, name, new_page_dir);
    if (!entry) {
        goto exec_failed;
    }

    uint32_t stack_top = MEM_TASK_STACK_TOP - MEM_TASK_ARG_SIZE;
    int err = alloc_mem_for_task(
        new_page_dir, MEM_TASK_STACK_SIZE / MEM_PAGE_SIZE,
        MEM_TASK_STACK_TOP - MEM_TASK_STACK_SIZE, PTE_P | PTE_U | PTE_W
    );
    if (err < 0) {
        goto exec_failed;
    }

    int argc = strings_count(argv);
    err = copy_args((uint8_t*)stack_top, new_page_dir, argc, argv);
    if (err < 0) {
        goto exec_failed;
    }

    syscall_frame_t *frame = (syscall_frame_t*)(task->tss.esp0 - sizeof(syscall_frame_t));
    frame->eip = entry;
    frame->eax = frame->ebx = frame->ecx = frame->edx = 0;
    frame->esi = frame->edi = frame->ebp = 0;
    frame->eflags = EFLAGS_IF | EFLAGS_DEFAULT;
    frame->esp = stack_top - sizeof(uint32_t) * SYSCALL_PARAM_COUNT;
    // cs ss are the same so not set

    task->tss.cr3 = new_page_dir;
    // should set cr3 to change page dir immediately
    mmu_set_page_dir(new_page_dir);
    // memory_destroy_uvm(old_page_dir); TODO: adding this line leads to error

    return 0;

exec_failed:
    if (new_page_dir) {
        memory_destroy_uvm(new_page_dir);
    }

    task->tss.cr3 = old_page_dir;
    mmu_set_page_dir(old_page_dir);

    return -1;
}

file_t *task_file(int fd) {
    file_t *file = (file_t*)0;
    if (fd >= 0 && fd < OPEN_FILE_NUM) {
        file = task_current()->file_table[fd];
    }

    return file; 
}

int task_alloc_fd(file_t *file) {
    task_t *task = task_current();
    for (int i = 0; i < OPEN_FILE_NUM; i++) {
        if (task->file_table[i]) {
            continue;
        }
        task->file_table[i] = file;
        return i;
    }
    
    return -1;
}

void task_remove_fd(int fd) {
    if (fd < 0 || fd >= OPEN_FILE_NUM) {
        return;
    }
    task_current()->file_table[fd] = (file_t*)0;
    return;
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
// why copying instead of mapping? both are fine

// 5. make tasks unable to access kernel code, otherwise it would be dangerous
// since we're using flat model (set segment regs as 0 and seg size as 0xFFFFFFFF)
// which means we can't simply utilize segmentation to protect kernel
// so we use the U/S bit of pte/pde to preovide protection
// U/S bit = 0 => privilege level 0, 1, 2 can access, U/S bit = 1 => privilege 3 can access

// 6. have to setup privilege level 0 stack and privilege level 3 stack for created tasks

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