#ifndef FS_H
#define FS_H

#include <sys/stat.h>
#include "fs/file.h"
#include "tools/list.h"
#include "ipc/mutex.h"
#include "fs/fatfs/fatfs.h"
#include "applib/lib_syscall.h"

#define FS_MOUNT_POINT_SIZE 128

struct _fs_t;

typedef struct _fs_op_t {
    int (*mount)(struct _fs_t *fs, int major, int minor);
    void (*unmount)(struct _fs_t *fs);
    int (*open)(struct _fs_t *fs, const char *path, file_t *file);
    int (*read)(char *buf, int size, file_t *file);
    int (*write)(char *buf, int size, file_t *file);
    void (*close)(file_t *file);
    int (*seek)(file_t *file, uint32_t offset, int dir);
    int (*stat)(file_t *file, struct stat *st);
    int (*ioctl)(file_t *file, int cmd, int arg0, int arg1);

    int (*opendir)(struct _fs_t *fs, const char *name, DIR *dir);
    int (*readdir)(struct _fs_t *fs, DIR *dir);
    int (*closedir)(struct _fs_t *fs, DIR *dir);
    int (*unlink)(struct _fs_t *fs, const char *file_name);
}fs_op_t;

typedef enum _fs_type_t {
    FS_DEVFS,
    FS_FAT16,
}fs_type_t;


typedef struct _fs_t {
    char mount_point[FS_MOUNT_POINT_SIZE];
    fs_type_t type;
    fs_op_t *op;
    void *data;
    // saved when mounting file system, so fs_open can just use it without doing it again
    int dev_id;
    // list_node_t node;
    mutex_t *mutex;

    union {
        fat_t fat_data;
    };
}fs_t;

void fs_init(void);

int sys_open(const char *name, int flags, ...); 
int sys_read(int file, char *ptr, int len);
int sys_write(int file, char *ptr, int len);
int sys_lseek(int file, int ptr, int dir);
int sys_close(int file);
int sys_isatty(int file);
int sys_fstat(int file, struct stat *st);
int sys_dup(int file);
int sys_ioctl (int file, int cmd, int arg0, int arg1);
int sys_unlink(const char *file_name);

int sys_opendir(const char *path, DIR *dir);
int sys_readdir(DIR *dir);
int sys_closedir(DIR *dir);


#endif


