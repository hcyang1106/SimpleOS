#include "fs/fs.h"
#include "tools/klib.h"
#include "comm/cpu_instr.h"
#include "os_cfg.h"
#include <sys/stat.h>
#include "tools/log.h"
#include "dev/console.h"
#include "dev/dev.h"
#include "core/task.h"
#include "fs/file.h"
#include <sys/file.h>
#include "dev/disk.h"
#include "applib/lib_syscall.h"

// shell is stored in the 5000th sector
// these functions are specifically for shell and are temparily simplified

#define FS_TABLE_SIZE 10
static fs_t fs_table[FS_TABLE_SIZE];

static uint8_t TEMP_ADDR[100*1024];
static uint8_t *temp_pos; // current position in file
#define TEMP_FILE_ID 100

extern fs_op_t devfs_op;
extern fs_op_t fatfs_op;

static fs_t *root_fs;

static void read_disk(uint32_t sector, uint32_t sector_count, uint8_t *buf) {
    outb(0x1F6, 0xE0); // fifth and seventh bit should be set to 1 (fixed usage), sixth bit is set to 1 to choose LBA mode
    outb(0x1F2, (uint8_t)(sector_count >> 8)); // first byte (from right)
    outb(0x1F3, (uint8_t)sector >> 24); // first byte (from right)
    outb(0x1F4, 0);
    outb(0x1F5, 0);

    outb(0x1F2, (uint8_t)sector_count); // second byte (from right)
    outb(0x1F3, (uint8_t)sector); // last byte (from right)
    outb(0x1F4, (uint8_t)(sector >> 8)); // third byte (from right)
    outb(0x1F5, (uint8_t)(sector >> 16)); // second byte (from right)

    outb(0x1F7, 0x24); // Send the "READ SECTORS EXT" command (0x24) to port 0x1F7

    uint16_t *data_buf = (uint16_t *)buf; // it reads 2 bytes at a time
    while (sector_count--) {
        while ((inb(0x1F7) & 0x88) != 0x8) {}// check if DRQ is one and BSY is zero
        for (int i = 0; i < SECTOR_SIZE / 2; i++) { 
            *data_buf++ = inw(0x1F0); // read 2 bytes at a time
        }
    }
}

static const char *next_path(const char *path) {
    // /dev/tty3
    const char *p = path;
    while (*p && *p == '/') { p++; }
    while (*p && *p != '/') { p++; }
    while (*p && *p == '/') { p++; }

    if (!*p) {
        return (const char*)0;
    }

    return p;
}

static void fs_protect(fs_t *fs) {
    if (fs->mutex) {
        mutex_lock(fs->mutex);
    }
}

static void fs_unprotect(fs_t *fs) {
    if (fs->mutex) {
        mutex_unlock(fs->mutex);
    }
}

static int is_invalid_fd(int fd) {
    if (fd < 0 && fd >= OPEN_FILE_NUM) {
        return -1;
    }
    return 0;
}

int sys_open(const char *name, int flags, ...) {
    // this reads from the disk to TEMP_ADDR buffer
    // and it reads from disk1 rather than disk2
    // later we modify the script so that it writes to disk2

    // if (kernel_strncmp(name, "/shell.elf", 3) == 0) {
    //     // read_disk(5000, 80, (uint8_t*)TEMP_ADDR);
    //     int dev_id = dev_open(DEV_DISK, 0xa0, (void*)0);
    //     // temp_pos is the current read position 
    //     temp_pos = (uint8_t*)TEMP_ADDR;
    //     dev_read(dev_id, 5000, TEMP_ADDR, 80);
        
    //     return TEMP_FILE_ID;
    // }

    file_t *file = file_alloc();
    if (!file) {
        goto sys_open_failed;
    }

    int fd = -1;
    fd = task_alloc_fd(file);
    if (fd < 0) {
        goto sys_open_failed;
    }

    fs_t *fs = (fs_t*)0;
    for (int i = 0; i < FS_TABLE_SIZE; i++) {
        fs_t *p = fs_table + i;
        if (kernel_strlen(p->mount_point) == 0 || kernel_strncmp(p->mount_point, name, kernel_strlen(p->mount_point)) != 0) {
            continue;
        }
        fs = p;
        break;
    }

    if (!fs) {
        // didn't find the mounted fs
        fs = root_fs;
    } else {
        name = next_path(name);
        if (!name) {
            goto sys_open_failed;
        }
    }

    // file->dev_id = -1; no need to set?
    file->mode = flags;
    file->fs = fs;
    // file->pos = 0; // tty does not use this, it uses cursor // already zero
    kernel_strncpy(file->file_name, name, FILE_NAME_SIZE);

    fs_protect(fs);
    int ret = fs->op->open(fs, name, file);
    if (ret < 0) {
        fs_unprotect(fs);
        goto sys_open_failed;
    }

    fs_unprotect(fs);
    return fd;

sys_open_failed:
    if (file) {
        file_free(file);
    }
    if (fd >= 0) {
        task_remove_fd(fd);
    }
    return -1;
}

// copy to specified mem position
// which layer should semaphore be put at?
// we need sem since we have "buffer"
// we design to make ttys have buffer,
// so it should be put in tty code instead of putting here
int sys_read(int file, char *ptr, int len) {
    // after readign to TEMP_ADDR (open), it reads to the ptr position
    // if (file == TEMP_FILE_ID) {
    //     kernel_memcpy(ptr, temp_pos, len);
    //     temp_pos += len;
    //     return len;
    // } 

    if (is_invalid_fd(file) || !ptr) {
        return -1;
    }

    if (!len) {
        return 0;
    }

    file_t *fp = task_file(file);
    if (!fp) {
        log_printf("file not opened");
        return -1;
    }

    if (fp->mode == O_WRONLY) {
        log_printf("file is write only");
        return -1;
    }

    // return dev_read(fp->dev_id, 0, ptr, len);
    fs_protect(fp->fs);
    int ret = fp->fs->op->read(ptr, len, fp);
    fs_unprotect(fp->fs);
    return ret;
}

// structure => printf, print_msg -> sys_write -> console_write
// log_printf -> console_write
// dev_write (tty_write) -> console_write
int sys_write(int file, char *ptr, int len) {
    // if (file == 1) {
    //     ptr[len] = '\0'; // this is only with log_printf, it checks '\0', not size
    //     log_printf("%s", ptr);
    //     // console_write(0, ptr, len); // this uses len so null ch is not needed
    // }
    if (is_invalid_fd(file) || !ptr) {
        return -1;
    }

    if (!len) {
        return 0;
    }

    file_t *fp = task_file(file);
    if (!fp) {
        log_printf("file not opened");
        return -1;
    }

    if (fp->mode == O_RDONLY) {
        log_printf("file is read only");
        return -1;
    }

    // return dev_read(fp->dev_id, 0, ptr, len);
    fs_protect(fp->fs);
    int ret = fp->fs->op->write(ptr, len, fp);
    fs_unprotect(fp->fs);
    return ret;
}

// ptr is the offset from start of file
int sys_lseek(int file, int ptr, int dir) {
    // move the position
    // if (file == TEMP_FILE_ID) {
    //     temp_pos = (uint8_t*)(TEMP_ADDR + ptr);
    //     return 0;
    // }

    if (is_invalid_fd(file)) {
        return -1;
    }

    file_t *fp = task_file(file);
    if (!fp) {
        log_printf("file not opened");
        return -1;
    }

    // return dev_read(fp->dev_id, 0, ptr, len);
    fs_protect(fp->fs);
    int ret = fp->fs->op->seek(fp, ptr, dir);
    fs_unprotect(fp->fs);
    return ret;
}

int sys_close(int file) {
    // if (file == TEMP_FILE_ID) {
    //     return 0;
    // }

    if (is_invalid_fd(file)) {
        return -1;
    }

    file_t *fp = task_file(file);
    if (!fp) {
        log_printf("file not opened");
        return -1;
    }

    task_remove_fd(file);

    if (--fp->ref > 0) {
        return 0;
    }

    fs_protect(fp->fs);
    fp->fs->op->close(fp);
    fs_unprotect(fp->fs);
    return 0;
}

int sys_isatty(int file) {
    if (is_invalid_fd(file)) {
        return -1;
    }

    file_t *fp = task_file(file);
    if (!fp) {
        log_printf("file not opened");
        return -1;
    }

    return fp->type == FILE_TTY;
}

int sys_fstat(int file, struct stat *st) {
    if (is_invalid_fd(file)) {
        return -1;
    }

    file_t *fp = task_file(file);
    if (!fp) {
        log_printf("file not opened");
        return -1;
    }

    kernel_memset(st, 0, sizeof(struct stat));

    fs_protect(fp->fs);
    int ret = fp->fs->op->stat(fp, st);
    fs_unprotect(fp->fs);
    return ret;
}

int sys_dup(int file) {
    if (is_invalid_fd(file)) {
        log_printf("file %d not valid", file);
        return -1;
    }

    file_t *fp = task_file(file);
    if (!fp) {
        log_printf("file not opened");
        return -1;
    }

    int fd = task_alloc_fd(fp);
    if (fd < 0) {
        log_printf("task file table full");
        return -1;
    }

    file_inc_ref(fp);
    // fp->ref++;
    return fd;
}

int sys_ioctl (int file, int cmd, int arg0, int arg1) {
    if (is_invalid_fd(file)) {
        log_printf("file %d not valid", file);
        return -1;
    }

    file_t *fp = task_file(file);
    if (!fp) {
        log_printf("file not opened");
        return -1;
    }

    int ret = -1;
    fs_protect(fp->fs);
    if (fp->fs->op->ioctl) {
        ret = fp->fs->op->ioctl(fp, cmd, arg0, arg1);
    }
    fs_unprotect(fp->fs);
    
    return ret;
}

static int fill_in_fs(fs_t *fs, fs_type_t type, char *mount_point) {
    kernel_memcpy(fs->mount_point, mount_point, FS_MOUNT_POINT_SIZE);
    fs->type = type;

    int ret = -1;
    switch (type) {
        case FS_FAT16:
            fs->op = &fatfs_op;
            ret = 0;
            break;
        case FS_DEVFS:
            fs->op = &devfs_op;
            ret = 0;
            break;
        default:
            break;
    }

    return ret;
}

static void unfill_fs(fs_t *fs) {
    kernel_memset(fs, 0, sizeof(fs_t));
    return;
}

static fs_t *mount(fs_type_t type, char *mount_point, int dev_major, int dev_minor) {
    log_printf("mounting file system, name=%s, dev_major=%d, dev_minor=%d", mount_point, dev_major, dev_minor);
    
    for (int i = 0; i < FS_TABLE_SIZE; i++) {
        fs_t *fs = fs_table + i;
        if (kernel_strncmp(mount_point, fs->mount_point, FS_MOUNT_POINT_SIZE) == 0) {
            log_printf("file system %s already mounted...", mount_point);
            return fs;
        }
    }

    fs_t *fs = (fs_t*)0;
    for (int i = 0; i < FS_TABLE_SIZE; i++) {
        fs_t *p = fs_table + i;
        if (*p->mount_point) {
            continue;
        }

        fs = p;
        
        int ret = fill_in_fs(fs, type, mount_point);
        if (ret < 0) {
            goto mount_failed;
        }
        
        ret = fs->op->mount(fs, dev_major, dev_minor);
        if (ret < 0) {
            goto mount_failed;
        }
        break;
    }
    return fs;

mount_failed:
    if (fs) {
        unfill_fs(fs);
    }
    return (fs_t*)0;
}

int sys_opendir(const char *path, DIR *dir) {
    fs_protect(root_fs);
    int ret = root_fs->op->opendir(root_fs, path, dir);
    fs_unprotect(root_fs);
    return ret;
}

// why not calling dev layer directly?
// => since there may be many different file systems
// => their behaviors may be different
int sys_readdir(DIR *dir) {
    fs_protect(root_fs);
    int ret = root_fs->op->readdir(root_fs, dir);
    fs_unprotect(root_fs);
    return ret;
}

int sys_closedir(DIR *dir) {
    fs_protect(root_fs);
    int ret = root_fs->op->closedir(root_fs, dir);
    fs_unprotect(root_fs);
    return ret;
}

int sys_unlink(const char *file_name) {
    fs_protect(root_fs);
    int ret = root_fs->op->unlink(root_fs, file_name);
    fs_unprotect(root_fs);
    return ret;
}

void fs_init(void) {
    disk_init();
    file_table_init();
    // i think we also need to pass into FS_DEVFS is because of efficiency
    // without this lead to many if and else if (comparison of strings)
    fs_t *fs = mount(FS_DEVFS, "/dev", 0, 0);
    ASSERT(fs != (fs_t*)0);
    fs = mount(FS_DEVFS, "/dev", 0, 0); // a test to mount twice
    ASSERT(fs != (fs_t*)0);

    root_fs = mount(FS_FAT16, "/home", ROOT_DEV);
    ASSERT(root_fs != (fs_t*)0);
}



