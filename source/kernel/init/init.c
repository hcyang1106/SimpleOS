#include "init.h"
#include "comm/boot_info.h"
#include "cpu/cpu.h"
#include "dev/time.h"
#include "tools/log.h"
#include "os_cfg.h"
#include "tools/klib.h"
#include "core/task.h"
#include "comm/cpu_instr.h"
#include "tools/list.h"

// static task_t main_task; relocate to task
static task_t init_task;
static uint32_t init_task_stack[1024];

void kernel_init(boot_info_t *boot_info) {
    ASSERT(boot_info->ram_region_count != 0);
    // ASSERT(3 < 2); // used to test ASSERT
    cpu_init();
    log_init();
    time_init();
    task_manager_init();
}

void init_task_entry(void) {
    int count = 0;
    for (;;) {
        log_printf("init task: %d", count++);
        // below is not a good way to switch because it the switch-to task is fixed
        // task_switch_from_to(&init_task, task_main_task());
        sys_sched_yield();
    }
}

// static void list_test(void) {
//     list_t list;
//     list_t *p_list = &list;
//     list_init(p_list);

//     log_printf("first_addr: 0x%x, last_addr: 0x%x, count: %d", \
//                 list_first(p_list), list_last(p_list), list_count(p_list));

//     list_node_t list_nodes[5];

//     list_init(p_list);
//     for (int i = 0; i < 5; i++) {
//         list_insert_first(p_list, &list_nodes[i]);
//     }
//     log_printf("---------------------------------------------------------");
//     log_printf("first_addr: 0x%x, last_addr: 0x%x, count: %d", \
//                 list_first(p_list), list_last(p_list), list_count(p_list));
//     list_node_t *it = p_list->first;
//     while (it) {
//         log_printf("0x%x", it);
//         it = it->next;
//     }
//     log_printf("---------------------------------------------------------");

//     list_init(p_list);
//     for (int i = 0; i < 5; i++) {
//         list_insert_last(p_list, &list_nodes[i]);
//     }
//     log_printf("---------------------------------------------------------");
//     log_printf("first_addr: 0x%x, last_addr: 0x%x, count: %d", \
//                 list_first(p_list), list_last(p_list), list_count(p_list));
//     it = p_list->first;
//     while (it) {
//         log_printf("0x%x", it);
//         it = it->next;
//     }
//     log_printf("---------------------------------------------------------");
    
//     // remove first five times
//     for (int i = 0; i < 5; i++) {
//         list_remove_first(p_list);
//         log_printf("removing => first_addr: 0x%x, last_addr: 0x%x, count: %d", \
//                 list_first(p_list), list_last(p_list), list_count(p_list));
//     }
//     // insert 5 again
//     for (int i = 0; i < 5; i++) {
//         list_insert_first(p_list, &list_nodes[i]);
//     }
//     log_printf("first_addr: 0x%x, last_addr: 0x%x, count: %d", \
//                 list_first(p_list), list_last(p_list), list_count(p_list));
//     // delete specific node five times
//     list_remove_node(p_list, &list_nodes[3]);
//     log_printf("removing => first_addr: 0x%x, last_addr: 0x%x, count: %d", \
//                 list_first(p_list), list_last(p_list), list_count(p_list));
//     list_remove_node(p_list, &list_nodes[3]);
//     log_printf("removing => first_addr: 0x%x, last_addr: 0x%x, count: %d", \
//                 list_first(p_list), list_last(p_list), list_count(p_list));
//     list_remove_node(p_list, &list_nodes[2]);
//     log_printf("removing => first_addr: 0x%x, last_addr: 0x%x, count: %d", \
//                 list_first(p_list), list_last(p_list), list_count(p_list));
//     list_remove_node(p_list, &list_nodes[1]);
//     log_printf("removing => first_addr: 0x%x, last_addr: 0x%x, count: %d", \
//                 list_first(p_list), list_last(p_list), list_count(p_list));
//     list_remove_node(p_list, &list_nodes[0]);
//     log_printf("removing => first_addr: 0x%x, last_addr: 0x%x, count: %d", \
//                 list_first(p_list), list_last(p_list), list_count(p_list));
//     list_remove_node(p_list, &list_nodes[4]);
//     log_printf("removing => first_addr: 0x%x, last_addr: 0x%x, count: %d", \
//                 list_first(p_list), list_last(p_list), list_count(p_list));
    
//     struct type_t {
//         int i;
//         list_node_t node;
//     }v = {0x123456};

//     ASSERT((uint32_t)&v == parent_addr(struct type_t, node, &v.node));
//     log_printf("parent pointer: %d", parent_pointer(struct type_t, node, &v.node)->i);
// }

void init_main() {
    log_printf("");

    // list_test();

    log_printf("Kernel is running...");
    log_printf("Version: %s", OS_VERSION);
    log_printf("%d, %d, %x, %c", 123456, -123, 0x80000000, 't');
    log_printf("%d", sizeof(unsigned long));

    // when this exception happens, it executes the instruction again
    // therefore if exception in kernel happens it stucks
    // int a = 3 / 0;
    // irq_enable_global(); // close it otherwise it is hard to debug (it will constantly run into time interrupt)
    
    // task_init(&main_task, 0, 0); // the main task doesn't need to set entry address and esp
    // stack grows from high to low, when inserting, first minus four then do the insert (so set as 1024)
    task_init(&init_task, "init task", (uint32_t)init_task_entry, (uint32_t)&init_task_stack[1024]);
    main_task_init();
    // write_tr(main_task.tss_sel);

    int count = 0;
    for (;;) {
        log_printf("init main: %d", count++);
        // task_switch_from_to(task_main_task(), &init_task);
        sys_sched_yield();
    }
}

// use hardware to switch tasks:
// 1. setup tss and gdt descriptor (for tss)
// 2. set task register as current tss selector
// 3. use jump instruction to jump to another tss (jump tss_sel)