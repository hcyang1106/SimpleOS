#ifndef SYSCALL_H
#define SYSCALL_H

#include "comm/types.h"

// this number is used to tell cpu to copy the last 5 elements
// in level 3 stack to level 0 stack
// lcall is used to call a function in a different segment
// lcall a call gate will copy the params from level 3 stack to level 0 stack
#define SYSCALL_PARAM_COUNT 5

#define SYS_msleep 0
#define SYS_getpid 1
#define SYS_fork 2
#define SYS_execve 3
#define SYS_yield 4
#define SYS_exit 5
#define SYS_wait 6

#define SYS_open 50
#define SYS_read 51
#define SYS_write 52
#define SYS_close 53
#define SYS_lseek 54
#define SYS_isatty 55
#define SYS_sbrk 56
#define SYS_fstat 57
#define SYS_dup 58
#define SYS_ioctl 59

#define SYS_opendir 60
#define SYS_readdir 61
#define SYS_closedir 62
#define SYS_unlink 63


#define SYS_print_msg 100


typedef struct {
    uint32_t eflags;
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, dummy, ebx, edx, ecx, eax; // (pusha) dummy is same as esp so not needed
    uint32_t eip, cs; // pushed by hardware
    uint32_t id, arg0, arg1, arg2, arg3; // pushed by hardware
    uint32_t esp, ss; // pushed by hardware
}syscall_frame_t;


void exception_handler_syscall (void);		// syscall处理

#endif












