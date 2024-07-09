#include "ipc/mutex.h"
#include "core/task.h"

void mutex_init(mutex_t *mutex) {
    mutex->locked_count = 0;
    mutex->owner = (task_t*)0;
    list_init(&mutex->wait_list);
}

void mutex_lock(mutex_t *mutex) {
    irq_state_t state = irq_enter_protection();

    task_t *curr = task_current();
    if (mutex->owner) {
        if (mutex->owner != curr) {
            // at first, i wrote sth like this and actually the right hand side
            // does not refer to the "outer" curr, it refers to the inner curr itself (shadowing)
            // task_t *curr = curr;
            task_set_unready(curr);
            list_insert_last(&mutex->wait_list, &curr->wait_node);
            task_dispatch();
        } else {
            mutex->locked_count++;
        }
    } else { // if using this as the first condition then there will not be double if
        mutex->locked_count++;
        mutex->owner = curr;
    }
    
    irq_leave_protection(state);
}

void mutex_unlock(mutex_t *mutex) {
    irq_state_t state = irq_enter_protection();

    if (mutex->owner == task_current()) {
        if (--mutex->locked_count == 0) {
            mutex->owner = (task_t*)0;
            if (list_count(&mutex->wait_list) > 0) {
                list_node_t *first = list_first(&mutex->wait_list);
                task_t *t = parent_pointer(task_t, wait_node, first);
                list_remove_first(&mutex->wait_list);
                task_set_ready(t);
                mutex->locked_count = 1;
                mutex->owner = t;
                task_dispatch();
            }
        }
    }

    irq_leave_protection(state);
}