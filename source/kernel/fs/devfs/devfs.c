#include "fs/devfs/devfs.h"
#include "fs/fs.h"
#include "dev/dev.h"
#include "tools/klib.h"
#include "tools/log.h"

static devfs_type_t devfs_type_list [] = {
    {
        .name = "tty",
        .dev_type = DEV_TTY,
        .file_type = FILE_TTY,
    }
};

int devfs_mount(fs_t *fs, int major, int minor) {
    return 0;
}

void devfs_unmount(fs_t *fs) {

}

static int path_to_num(const char *num, int *minor) {
    const char *p = num;

    int n = 0;
    while (*p) {
        n = n * 10 + (*p - '0');
        p++;
    }

    *minor = n;
    if (*minor < 0) {
        return -1;
    }

    return 0;
}

int devfs_open(fs_t *fs, const char *path, file_t *file) {
    for (int i = 0; i < sizeof(devfs_type_list) / sizeof(devfs_type_list[0]); i++) {
        devfs_type_t *type = devfs_type_list + i;
        int len = kernel_strlen(type->name);
        if (kernel_strncmp(type->name, path, len) != 0) {
            continue;
        }

        int minor = -1;
        if (kernel_strlen(path) < kernel_strlen(type->name) || path_to_num(path + len, &minor) < 0) {
            log_printf("get device number failed, path=%s", path);
            return -1;
        }
        
        int dev_id = dev_open(type->dev_type, minor, (void*)0);
        if (dev_id < 0) {
            log_printf("open device failed, path=%s", path);
            return -1;
        }

        file->dev_id = dev_id;
        file->type = type->file_type;

        break;
    }

    return 0;
}

int devfs_read(char *buf, int size, file_t *file) {
    return dev_read(file->dev_id, file->pos, buf, size);
}

int devfs_write(char *buf, int size, file_t *file) {
    return dev_write(file->dev_id, file->pos, buf, size);
}

void devfs_close(file_t *file) {
    dev_close(file->dev_id);
}

int devfs_seek(file_t *file, uint32_t offset, int dir) {
    return -1;
}

int devfs_stat(file_t *file, struct stat *st) {
    return -1;
}

int devfs_ioctl(file_t *file, int cmd, int arg0, int arg1) {
    return dev_control(file->dev_id, cmd, arg0, arg1);
}

fs_op_t devfs_op = {
    .mount = devfs_mount,
    .unmount = devfs_unmount,
    .open = devfs_open,
    .close = devfs_close,
    .read = devfs_read,
    .write = devfs_write,
    .seek = devfs_seek,
    .stat = devfs_stat,
    .ioctl = devfs_ioctl,
};