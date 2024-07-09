#include "ipc/sem.h"
#include "tools/list.h"
#include "core/task.h"
#include "cpu/cpu.h"

void sem_init(sem_t *sem, int count) {
    sem->count = count;
    list_init(&sem->wait_list);
}

void sem_wait(sem_t *sem) {
    // sem can be accessed at the same time, so need protection
    irq_state_t state = irq_enter_protection();

    if (sem->count > 0) {
        sem->count--;
    } else {
        task_t *curr = task_current();
        task_set_unready(curr);
        list_insert_last(&sem->wait_list, &curr->wait_node);
        task_dispatch();
    }

    irq_leave_protection(state);
}

void sem_notify(sem_t *sem) {
    irq_state_t state = irq_enter_protection();

    if (list_count(&sem->wait_list) > 0) {
        list_node_t *node = list_first(&sem->wait_list);
        list_remove_first(&sem->wait_list);
        task_t *p = parent_pointer(task_t, wait_node, node);
        task_set_ready(p);
    } else {
        sem->count++;
    }

    irq_leave_protection(state);
}