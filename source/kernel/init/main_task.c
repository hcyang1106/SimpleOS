// here we're trying to separate the main task code from kernel code
#include "core/task.h"
#include "tools/log.h"

int main_task(void) {
    for (;;) {
        log_printf("main task");
        sys_sleep(1000);
    }

    return 0;
}