#ifndef LIB_SYSCALL_H
#define LIB_SYSCALL_H

#include "os_cfg.h"
#include "comm/types.h"
#include "core/syscall.h"
#include <sys/stat.h>

typedef struct _syscall_args_t {
    uint32_t id;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t arg3;
}syscall_args_t;

void msleep(int ms);
uint32_t getpid(void);
uint32_t fork(void);
void print_msg(const char *fmt, int arg);
// interpret char *const *argv: argv is a pointer points to (char *const)
int execve(const char *name, char *const *argv, char *const *env);
int yield(void);

int open(const char *name, int flags, ...);
int read(int file, char *ptr, int len);
int write(int file, char *ptr, int len);
int close(int file);
int lseek(int file, int ptr, int dir);
int ioctl(int file, int cmd, int arg0, int arg1);
int unlink(const char *file_name);

int isatty(int file);
int fstat(int files, struct stat *st);
void *sbrk(ptrdiff_t incr);
int dup(int file);
void _exit(int status);
int wait(int *status);

struct dirent {
    int index;
    int type;
    char name[255];
    int size;
};

typedef struct _DIR {
    int index;
    struct dirent dirent;
}DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

#endif

// cpl is the current privilege level, rpl is the requested privilege level
// generally, max(cpl, rpl) <= dpl

