#include "core/syscall.h"
#include "core/task.h"
#include "tools/log.h"
#include "fs/fs.h"
#include "core/memory.h"

typedef int (*syscall_handler_t)(
    uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3
);

void sys_print_msg(const char *fmt, int arg) {
    log_printf(fmt, arg);
}

// sys_handler_t is actually a function pointer type
static const syscall_handler_t sys_table[] = {
    [SYS_msleep] = (syscall_handler_t)sys_msleep,
    [SYS_getpid] = (syscall_handler_t)sys_getpid,
    [SYS_fork] = (syscall_handler_t)sys_fork,
    [SYS_execve] = (syscall_handler_t)sys_execve,
    [SYS_print_msg] = (syscall_handler_t)sys_print_msg,
    [SYS_yield] = (syscall_handler_t)sys_yield,
    [SYS_wait] = (syscall_handler_t)sys_wait,
    [SYS_exit] = (syscall_handler_t)sys_exit,
    [SYS_open] = (syscall_handler_t)sys_open,
    [SYS_read] = (syscall_handler_t)sys_read,
    [SYS_write] = (syscall_handler_t)sys_write,
    [SYS_close] = (syscall_handler_t)sys_close,
    [SYS_lseek] = (syscall_handler_t)sys_lseek,
    [SYS_isatty] = (syscall_handler_t)sys_isatty,
    [SYS_sbrk] = (syscall_handler_t)sys_sbrk,
    [SYS_fstat] = (syscall_handler_t)sys_fstat,
    [SYS_dup] = (syscall_handler_t)sys_dup,
    [SYS_opendir] = (syscall_handler_t)sys_opendir,
    [SYS_readdir] = (syscall_handler_t)sys_readdir,
    [SYS_closedir] = (syscall_handler_t)sys_closedir,
    [SYS_ioctl] = (syscall_handler_t)sys_ioctl,
    [SYS_unlink] = (syscall_handler_t)sys_unlink,
};

void do_handler_syscall(syscall_frame_t *frame) {
    if (frame->id < sizeof(sys_table) / sizeof(sys_table[0])) {
        syscall_handler_t handler = sys_table[frame->id];
        if (handler) {
            int ret = handler(frame->arg0, frame->arg1, frame->arg2, frame->arg3);
            frame->eax = ret;
            return;
        }
    }
    
    task_t *task = task_current();
    log_printf("Task: %s, Unknown syscall: %d", task->name, frame->id);
    frame->eax = -1; // return -1 if unsuccess
    return;
}



