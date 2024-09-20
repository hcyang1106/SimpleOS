#include "lib_syscall.h"
#include "stdlib.h"

// cpl is the current privilege level, rpl is the requested privilege level
// generally, max(cpl, rpl) <= dpl
static inline uint32_t sys_call(syscall_args_t *args) {
    uint32_t ret;
    // first 0 is the offset, we set to zero because offset is saved in descriptor
    // 3 is RPL, value should be smaller or equal to DPL (0, 1, 2) is ok
    // DPL should be 3 because CPL is 3 and CPL <= DPL
    // uint32_t addr[] = {0, SELECTOR_SYSCALL | 3};
    uint32_t addr[] = {0, SELECTOR_SYSCALL | 0};
    __asm__ __volatile__ (
        "push %[arg3]\n\t"
        "push %[arg2]\n\t"
        "push %[arg1]\n\t"
        "push %[arg0]\n\t"
        "push %[id]\n\t"
        "lcall *(%[a])":
        "=a"(ret):
        [arg3]"r"(args->arg3),
        [arg2]"r"(args->arg2),
        [arg1]"r"(args->arg1),
        [arg0]"r"(args->arg0),
        [id]"r"(args->id),
        [a]"r"(addr)
    );

    return ret;
}

void msleep(int ms) {
    if (ms <= 0) {
        return;
    }

    syscall_args_t args;
    args.id = SYS_msleep;
    args.arg0 = ms;

    sys_call(&args);
}

uint32_t getpid(void) {
    syscall_args_t args;
    args.id = SYS_getpid;

    return sys_call(&args);
}

uint32_t fork(void) {
    syscall_args_t args;
    args.id = SYS_fork;

    return sys_call(&args);
}

void print_msg(const char *fmt, int arg) {
    syscall_args_t args;
    args.id = SYS_print_msg;
    args.arg0 = (uint32_t)fmt;
    args.arg1 = arg; // implicit type conversion

    sys_call(&args);
}

// interpret char *const *argv: argv is a pointer points to (char *const)
int execve(const char *name, char *const *argv, char *const *env) {
    syscall_args_t args;
    args.id = SYS_execve;
    args.arg0 = (uint32_t)name;
    args.arg1 = (uint32_t)argv;
    args.arg2 = (uint32_t)env;

    return sys_call(&args);
}

int yield(void) {
    syscall_args_t args;
    args.id = SYS_yield;

    return sys_call(&args);
}

int open(const char *name, int flags, ...) {
    syscall_args_t args;
    args.id = SYS_open;
    args.arg0 = (uint32_t)name;
    args.arg1 = (uint32_t)flags;

    return sys_call(&args);
}

int read(int file, char *ptr, int len) {
    syscall_args_t args;
    args.id = SYS_read;
    args.arg0 = (uint32_t)file;
    args.arg1 = (uint32_t)ptr;
    args.arg2 = (uint32_t)len;

    return sys_call(&args);
}

int write(int file, char *ptr, int len) {
    syscall_args_t args;
    args.id = SYS_write;
    args.arg0 = (uint32_t)file;
    args.arg1 = (uint32_t)ptr;
    args.arg2 = (uint32_t)len;

    return sys_call(&args);
}

int close(int file) {
    syscall_args_t args;
    args.id = SYS_close;
    args.arg0 = (uint32_t)file;

    return sys_call(&args);
}

int lseek(int file, int ptr, int dir) {
    syscall_args_t args;
    args.id = SYS_lseek;
    args.arg0 = (uint32_t)file;
    args.arg1 = (uint32_t)ptr;
    args.arg2 = (uint32_t)dir;

    return sys_call(&args);
}

int isatty(int file) {
    syscall_args_t args;
    args.id = SYS_isatty;
    args.arg0 = (uint32_t)file;

    return sys_call(&args);
}

int fstat(int files, struct stat *st) {
    syscall_args_t args;
    args.id = SYS_fstat;
    args.arg0 = (uint32_t)files;
    args.arg1 = (uint32_t)st;

    return sys_call(&args);
}

void *sbrk(ptrdiff_t incr) {
    syscall_args_t args;
    args.id = SYS_sbrk;
    args.arg0 = (uint32_t)incr;

    return (void*)sys_call(&args);
}

int dup(int file) {
    syscall_args_t args;
    args.id = SYS_dup;
    args.arg0 = (uint32_t)file;

    return sys_call(&args);
}

void _exit(int status) {
    syscall_args_t args;
    args.id = SYS_exit;
    args.arg0 = (uint32_t)status;

    sys_call(&args);
}

int wait(int *status) {
    syscall_args_t args;
    args.id = SYS_wait;
    args.arg0 = (uint32_t)status;

    return sys_call(&args);
}



