#ifndef SEM_H
#define SEM_H

#include "tools/list.h"

typedef struct {
    int count;
    list_t wait_list;
}sem_t;

void sem_init(sem_t *sem, int count);
void sem_wait(sem_t *sem);
void sem_notify(sem_t *sem);

#endif


// in sem.c, we can modify values in sem_t for sure
// however, for other source files, we provide functions
// for them to use instead of letting them modify it directly?

// task_current need to be protected?