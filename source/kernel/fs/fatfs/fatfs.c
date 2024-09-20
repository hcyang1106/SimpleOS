#include "fs/fs.h" 
#include "fs/fatfs/fatfs.h"
#include "dev/dev.h"
#include "tools/log.h"
#include "core/memory.h"
#include "os_cfg.h"
#include "tools/klib.h"
#include <sys/fcntl.h>

int fatfs_mount(struct _fs_t *fs, int major, int minor) {
    int dev_id = dev_open(major, minor, (void*)0);
    if (dev_id < 0) {
        log_printf("dev open failed, dev=%d", dev_id);
        goto mount_failed;
    }

    // this page will later become a buffer
    dbr_t *dbr = (dbr_t*)mem_alloc_page(1);
    if (!dbr) {
        log_printf("mem alloc page failed during mount");
        goto mount_failed;
    }

    int ret = dev_read(dev_id, 0, (char*)dbr, 1);
    if (ret < 0) {
        log_printf("dev read failed, dev=%d", dev_id);
        goto mount_failed;
    }

    fat_t *fat = &fs->fat_data;
    fat->bytes_per_sec = dbr->BPB_BytsPerSec;
    fat->tbl_start = dbr->BPB_RsvdSecCnt;
    fat->tbl_sectors = dbr->BPB_FATSz16;
    fat->tbl_cnt = dbr->BPB_NumFATs;
    fat->root_ent_cnt = dbr->BPB_RootEntCnt;
    fat->sec_per_cluster = dbr->BPB_SecPerClus;
    fat->root_start = fat->tbl_start + fat->tbl_sectors * fat->tbl_cnt;
    fat->data_start = fat->root_start + fat->root_ent_cnt * 32 / fat->bytes_per_sec;
    fat->cluster_byte_size = fat->bytes_per_sec * fat->sec_per_cluster;
    fat->fat_buffer = (uint8_t*)dbr;

    fat->fs = fs;

    if (fat->tbl_cnt != 2) {
        log_printf("fat table num error, major: %x, minor: %x", major, minor);
		goto mount_failed;
	}

    if (kernel_memcmp(dbr->BS_FileSysType, "FAT16", 5) != 0) {
        log_printf("not a fat16 file system, major: %x, minor: %x", major, minor);
        goto mount_failed;
    }

    fs->dev_id = dev_id;
    fs->data = &fs->fat_data;

    return 0;

mount_failed:
    if (dbr) {
        mem_free_page((uint32_t)dbr, 1);
    }
    if (dev_id >=0) {
        dev_close(dev_id);
    }
    return -1;
}

void fatfs_unmount(struct _fs_t *fs) {
    fat_t *fat = (fat_t*)fs->data;
    dev_close(fs->dev_id);
    mem_free_page((uint32_t)fat->fat_buffer, 1);
}

static file_type_t diritem_get_type(diritem_t *item) {
    if (item->DIR_Attr & DIRITEM_ATTR_HIDDEN || 
               item->DIR_Attr & DIRITEM_ATTR_SYSTEM ||
               item->DIR_Attr & DIRITEM_ATTR_VOLUME_ID
    ) {
        return FILE_UNKNOWN;
    }

    if (item->DIR_Attr & DIRITEM_ATTR_DIRECTORY) {
        return FILE_DIR;
    } else {
        return FILE_NORMAL;
    }
}

static void read_item_to_file(fat_t *fat, diritem_t *item, file_t *file, int index) {
    file->type = diritem_get_type(item);
    file->size = item->DIR_FileSize;
    file->pos = 0;
    file->p_index = index;
    file->sblk = (item->DIR_FstClusHI << 16) | item->DIR_FstClusL0;
    file->cblk = file->sblk;
}

static int read_dir_entry(fat_t *fat, int index, diritem_t *item) {
    if (index < 0 || index >= fat->root_ent_cnt) {
        log_printf("invalid dir index");
        return -1;
    }

    int sector_idx = fat->root_start + index * sizeof(diritem_t) / fat->bytes_per_sec;
    int offset = (index * sizeof(diritem_t)) % fat->bytes_per_sec;
    // if (fat->sector_idx == sector_idx) {
    //     // % fat->bytes_per_sec is needed!
    //     *item = (diritem_t*)(fat->fat_buffer + (index * sizeof(diritem_t)) % fat->bytes_per_sec);
    //     return 0;
    // }

    int ret = dev_read(fat->fs->dev_id, sector_idx, fat->fat_buffer, 1);
    if (ret != 1) {
        return -1;
    }

    fat->sector_idx = sector_idx;
    kernel_memcpy(item, (fat->fat_buffer + offset), sizeof(diritem_t));
    return 0;
}

static void to_format_name (const char *path, char *format_name) {
    int i = 0;
    while (*path) {
        if (*path == '.') {
            for (int j = i; j < 8; j++) {
                format_name[j] = ' ';
            }
            i = 8;
            path++;
            continue;
        }
        format_name[i] = *path - 'a' + 'A';
        i++;
        path++;
    }
    for (int j = i; j < FORMAT_NAME_LEN; j++) {
        format_name[j] = ' ';
    }
}

// ex: from file.c to FILE____C__
// here we change normal form to short form
int diritem_name_match(const char *path, diritem_t *item) {
    char buf[FORMAT_NAME_LEN];
    to_format_name(path, buf);
    return kernel_memcmp(item->DIR_Name, buf, FORMAT_NAME_LEN) == 0;
}

static int write_dir_entry(fat_t *fat, diritem_t *item, int index) {
    if (index < 0 || index >= fat->root_ent_cnt) {
        log_printf("invalid dir index");
        return -1;
    }

    int sector_idx = fat->root_start + index * sizeof(diritem_t) / fat->bytes_per_sec;
    int offset = (index * sizeof(diritem_t)) % fat->bytes_per_sec;
    // cache hit
    // if (fat->sector_idx == sector_idx) {
    //     // this line doesn't work and reason remains unknown
    //     // *(diritem_t*)(fat->fat_buffer + (index * sizeof(diritem_t)) % fat->bytes_per_sec) = *item;
    //     kernel_memcpy(fat->fat_buffer + (index * sizeof(diritem_t)) % fat->bytes_per_sec, item, sizeof(diritem_t));
    //     int ret = dev_write(fat->fs->dev_id, sector_idx, fat->fat_buffer, 1);
    //     if (ret != 1) {
    //         return -1;
    //     }
    //     return 0;
    // }

    // cache miss
    // read from disk to cache first
    int ret = dev_read(fat->fs->dev_id, sector_idx, fat->fat_buffer, 1);
    if (ret != 1) {
        return -1;
    }
    fat->sector_idx = sector_idx;

    // write to cache and write to disk
    // *(diritem_t*)(fat->fat_buffer + (index * sizeof(diritem_t)) % fat->bytes_per_sec) = *item;
    kernel_memcpy(fat->fat_buffer + offset, item, sizeof(diritem_t));
    ret = dev_write(fat->fs->dev_id, sector_idx, fat->fat_buffer, 1);
    if (ret != 1) {
        return -1;
    }
    return 0;
}

static void diritem_init(diritem_t *item, uint8_t attr, const char *name) {
    to_format_name(name, item->DIR_Name);
    item->DIR_FstClusHI = 0; // since cluster index is only 16 bits long
    item->DIR_FstClusL0 = (uint16_t)(FAT_CLUSTER_INVALID & 0xFFFF);
    item->DIR_FileSize = 0;
    item->DIR_Attr = attr;
    item->DIR_NTRes = 0;
    item->DIR_CrtTime = 0;
    item->DIR_CrtDate = 0;
    item->DIR_WrtTime = item->DIR_CrtTime;
    item->DIR_WrtDate = item->DIR_CrtDate;
    item->DIR_LastAccDate = item->DIR_CrtDate;
}

static int cluster_set_next(fat_t *fat, uint16_t cluster_num, uint16_t next) {
    int offset = cluster_num * sizeof(uint16_t);
    int sector_idx = fat->tbl_start + offset / fat->bytes_per_sec;
    int sector_offset = offset % fat->bytes_per_sec;

    // hit
    // if (fat->sector_idx == sector_idx) {
    //     *(uint16_t*)(fat->fat_buffer + sector_offset) = next;
    //     int ret = dev_write(fat->fs->dev_id, sector_idx, fat->fat_buffer, 1);
    //     if (ret < 0) {
    //         return ret;
    //     }
    //     return 0;
    // }

    // miss
    // read from disk 
    int ret = dev_read(fat->fs->dev_id, sector_idx, fat->fat_buffer, 1);
    if (ret < 0) {
        return ret;
    }
    fat->sector_idx = sector_idx;

    // write buffer, write disk
    *(uint16_t*)(fat->fat_buffer + sector_offset) = next;
    ret = dev_write(fat->fs->dev_id, sector_idx, fat->fat_buffer, 1);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

// read the corresponding sector to fat buffer 
// and get the next cluster number
uint16_t cluster_get_next(fat_t *fat, int curr_block) {
    // an entry is 16 bit long
    int offset = curr_block * sizeof(uint16_t);
    int sector_idx = fat->tbl_start + offset / fat->bytes_per_sec;
    // if (fat->sector_idx == sector_idx) {
    //     return *(uint16_t*)(fat->fat_buffer + offset % fat->bytes_per_sec); 
    // }

    int ret = dev_read(fat->fs->dev_id, sector_idx, fat->fat_buffer, 1);
    if (ret < 0) {
        return -1;
    }

    fat->sector_idx = sector_idx;
    return *(uint16_t*)(fat->fat_buffer + offset % fat->bytes_per_sec);
}

int cluster_invalid(uint16_t cluster) {
    if (cluster >= FAT_CLUSTER_INVALID || cluster < 2) {
        return 1;
    }
    return 0;
}

static int cluster_free_chain(fat_t *fat, uint16_t cluster_num) {
    while (!cluster_invalid(cluster_num)) {
        uint16_t next = cluster_get_next(fat, cluster_num);
        int ret = cluster_set_next(fat, cluster_num, FAT_CLUSTER_FREE);
        if (ret < 0) {
            log_printf("write fat table failed");
            return -1;
        }
        cluster_num = next;
    }
    return 0;
}

static uint16_t cluster_alloc_free(fat_t *fat, int count) {
    if (!count) {
        return FAT_CLUSTER_INVALID;
    }

    uint16_t start = FAT_CLUSTER_INVALID;
    uint16_t pre = FAT_CLUSTER_INVALID;
    int fat_tbl_len = fat->tbl_sectors * fat->bytes_per_sec / sizeof(uint16_t);
    for (uint16_t i = 2; count > 0 && i < fat_tbl_len; i++) {
        uint16_t next = cluster_get_next(fat, i);
        if (next == FAT_CLUSTER_FREE) {
            if (cluster_invalid(start)) {
                start = i;
                pre = i;
                count--;
                continue;
            }

            int ret = cluster_set_next(fat, pre, i);
            if (ret < 0) {
                cluster_free_chain(fat, start);
                return FAT_CLUSTER_INVALID;
            }
            pre = i;
            count--;
        }
    }

    if (count > 0) {
        cluster_free_chain(fat, start);
        return FAT_CLUSTER_INVALID;
    }

    int ret = cluster_set_next(fat, pre, FAT_CLUSTER_INVALID);
    if (ret < 0) {
        cluster_free_chain(fat, start);
        return FAT_CLUSTER_INVALID;
    }

    return start;
}

// after this function the file size still remains the same
// but the clusters are expanded
static int expand_file(file_t *file, int inc_size) {
    fat_t *fat = (fat_t*)file->fs->data;
    int cluster_offset = (file->size - 1) % fat->cluster_byte_size;
    int cluster_remain = (fat->cluster_byte_size - 1) - cluster_offset;
    if (file->pos != 0 && file->pos == file->size && file->pos % fat->cluster_byte_size == 0) {
        cluster_remain = fat->cluster_byte_size;
    }

    if (inc_size > cluster_remain) {
        int cluster_extra_need = up(inc_size - cluster_remain, fat->cluster_byte_size) / fat->cluster_byte_size;

        uint16_t start = cluster_alloc_free(fat, cluster_extra_need);
        if (cluster_invalid(start)) {
            return -1;
        }
        if (cluster_invalid(file->sblk)) {
            file->sblk = file->cblk = start;
            return 0;
        } 
        int ret = cluster_set_next(fat, file->cblk, start);
        if (ret < 0) {
            return -1;
        }
        return 0;
    }

    return 0;
}

// fill in the necessary
int fatfs_open(struct _fs_t *fs, const char *path, file_t *file) {
    fat_t *fat = (fat_t*)fs->data;
    diritem_t *item = (diritem_t*)0;
    diritem_t p_item;
    int found_index = -1;
    int free_index = -1;
    for (int i = 0; i < fat->root_ent_cnt; i++) {
        int ret = read_dir_entry(fat, i, &p_item);
        if (ret < 0) {
            return -1;
        }
        if (p_item.DIR_Name[0] == DIRITEM_NAME_END) {
            if (free_index == -1) {
                free_index = i;
            }
            break;
        }
        if (p_item.DIR_Name[0] == DIRITEM_NAME_FREE) {
            free_index = i;
            continue;
        }
        if(diritem_name_match(path, &p_item)) {
            found_index = i;
            item = &p_item;
            break;
        }
    }

    if (item) {
        read_item_to_file(fat, item, file, found_index);
        if (file->mode & O_TRUNC) {
            cluster_free_chain(fat, file->sblk);
            file->sblk = file->cblk = FAT_CLUSTER_INVALID;
            file->size = 0;
            file->pos = 0;
        }
        return 0;
    }
    
    if (file->mode & O_CREAT && free_index != -1) {
        diritem_t new_item;
        diritem_init(&new_item, 0, path);
        // write new_item (in memory) into the item table (disk)
        int ret = write_dir_entry(fat, &new_item, free_index);
        if (ret < 0) {
            log_printf("create new file failed");
            return ret;
        }
        read_item_to_file(fat, &new_item, file, free_index);
        return 0;
    }
    
    return -1;
}

// since file pos is the position where the next byte is going to write at,
// it is possible that after expanding file in fatfs_write, the eventual file pos 
// still needs an extra cluster
static int move_file_pos(file_t *file , int move_bytes, fat_t *fat, int expand) {
    int offset = file->pos % fat->cluster_byte_size;
    if (offset + move_bytes >= fat->cluster_byte_size) {
        uint16_t next = cluster_get_next(fat, file->cblk);
        if ((next == FAT_CLUSTER_INVALID) && expand) {
            int ret = expand_file(file, fat->cluster_byte_size);
            if (ret < 0) {
                return ret;
            }
            next = cluster_get_next(fat, file->cblk);;
        }
        file->cblk = next;
    }

    file->pos += move_bytes;
    return 0;
}
 
// notice that data cluster index starts from "2"
// after completing this function i found that in fatfs_read and write the unit is cluster
// however when reading and writing fat table and directory items we use sector as unit
// why fatfs_read/write use cluster as unit? => since the fat table is designed for cluster number
int fatfs_read(char *buf, int size, file_t *file) {
    fat_t *fat = (fat_t*)file->fs->data;

    int nbytes = size; // bytes remaining
    if (file->pos + nbytes > file->size) {
        nbytes = file->size - file->pos;
    }

    int total = 0;
    while (nbytes > 0) {
        // pos start is the start of a cluster
        int cluster_offset = file->pos % fat->cluster_byte_size;
        // this start sector is the "first sector of a specific cluster"
        // it is the sector where the "file position" locates at
        int start_sector = fat->data_start + fat->sec_per_cluster * (file->cblk - 2);
        if (cluster_offset == 0 && nbytes >= fat->cluster_byte_size) {
            int ret = dev_read(fat->fs->dev_id, start_sector, buf, fat->sec_per_cluster);
            if (ret < 0) {
                log_printf("read error in fatfs read");
                return total;
            }
            nbytes -= fat->cluster_byte_size;
            buf += fat->cluster_byte_size;
            total += fat->cluster_byte_size;
            ret = move_file_pos(file, fat->cluster_byte_size, fat, 0);
            if (ret < 0) {
                log_printf("move file position failed");
                return total;
            }
            continue;
        }

        int cluster_remain = fat->cluster_byte_size - cluster_offset;
        int read_bytes = (nbytes > cluster_remain) ? cluster_remain : nbytes;
        fat->sector_idx = start_sector;
        int ret = dev_read(fat->fs->dev_id, start_sector, fat->fat_buffer, fat->sec_per_cluster);
        if (ret < 0) {
            log_printf("read error in fatfs read");
            return total;
        }

        kernel_memcpy(buf, fat->fat_buffer + cluster_offset, read_bytes);

        buf += read_bytes;
        total += read_bytes;
        nbytes -= read_bytes;
        ret = move_file_pos(file, read_bytes, fat, 0);
        if (ret < 0) {
            log_printf("move file position failed");
            return total;
        }
    }

    return total;
}

// fat write unit is cluster
// protection?
// if file pos is at the location where the new chararcter is going to write at
// then file pos == file size
int fatfs_write(char *buf, int size, file_t *file) {
    fat_t *fat = (fat_t*)file->fs->data;

    if (file->pos + size > file->size) { // pos <= size - 1
        int inc_size = file->pos + size - file->size;
        int ret = expand_file(file, inc_size);
        if (ret < 0) {
            return 0; // this function returns how many bytes written
        }
    }

    int total_write = 0;
    int nbytes = size;
    while (nbytes) {
        int cluster_offset = file->pos % fat->cluster_byte_size;
        int start_sector = fat->data_start + fat->sec_per_cluster * (file->cblk - 2);
        if (cluster_offset == 0 && nbytes >= fat->cluster_byte_size) {
            // if (fat->sector_idx == start_sector) {
            //     kernel_memcpy(fat->fat_buffer, buf, fat->cluster_byte_size);
            // }
            int ret = dev_write(fat->fs->dev_id, start_sector, buf, fat->sec_per_cluster);
            if (ret < 0) {
                log_printf("dev write failed during fatfs write");
                return total_write;
            }

            total_write += fat->cluster_byte_size;
            nbytes -= fat->cluster_byte_size;
            ret = move_file_pos(file, fat->cluster_byte_size, fat, 1);
            if (ret < 0) {
                log_printf("dev write failed during fatfs write");
                return total_write;
            }
            continue;
        }

        int cluster_remain = fat->cluster_byte_size - cluster_offset;
        int write_bytes = (nbytes > cluster_remain) ? cluster_remain : nbytes;
        // if (fat->sector_idx != start_sector) {
            fat->sector_idx = start_sector;
            int err = dev_read(fat->fs->dev_id, start_sector, fat->fat_buffer, fat->sec_per_cluster);
            if (err < 0) {
                return total_write;
            }
        //}

        kernel_memcpy(fat->fat_buffer + cluster_offset, buf, write_bytes);
            
        int ret = dev_write(fat->fs->dev_id, start_sector, fat->fat_buffer, fat->sec_per_cluster);
        if (ret < 0) {
            log_printf("dev write failed during fatfs write");
            return total_write;
        }

        total_write += write_bytes;
        nbytes -= write_bytes;
        ret = move_file_pos(file, write_bytes, fat, 1);
        if (ret < 0) {
            log_printf("dev write failed during fatfs write");
            return total_write;
        }
    }

    file->size = (file->pos > file->size) ? file->pos : file->size;
    return total_write;
}

int fatfs_opendir(struct _fs_t *fs, const char *name, DIR *dir) {
    dir->index = 0;
    return 0;
}

static void diritem_get_name(char *dirent_name, diritem_t *item) {
    char *src = item->DIR_Name;
    char *dest = dirent_name;
    for (int i = 0; i < 8; i++) {
        if (*src == ' ') {
            src++;
            continue;
        }
        *dest = *src;
        src++;
        dest++;
    }
    
    *dest = '.';
    dest++;

    for (int i = 0; i < 3; i++) {
        if (*src == ' ') {
            break;
        }
        *dest = *src;
        src++;
        dest++;
    }

    if (*(dest - 1) == '.') {
        *(dest - 1) = '\0';
    } else {
        *dest = '\0';
    }
}

int fatfs_readdir(struct _fs_t *fs, DIR *dir) {
    fat_t *fat = (fat_t*)fs->data;
    while (dir->index < fat->root_ent_cnt) {
        diritem_t item;
        int ret = read_dir_entry(fat, dir->index, &item);
        if (ret < 0) {
            log_printf("read dir entry failed");
            return -1;
        }
        
        if (item.DIR_Name[0] == DIRITEM_NAME_END) {
            break;
        }

        if (item.DIR_Name[0] == DIRITEM_NAME_FREE) {
            dir->index++;
            continue;
        }

        dir->dirent.type = diritem_get_type(&item);
        if (dir->dirent.type != FILE_DIR && dir->dirent.type != FILE_NORMAL) {
            dir->index++;
            continue;
        }

        dir->dirent.index = dir->index;
        diritem_get_name(dir->dirent.name, &item);
        dir->dirent.size = item.DIR_FileSize;
        dir->index++;
        return 0;
    }

    return -1;
}

int fatfs_closedir(struct _fs_t *fs, DIR *dir) {
    return 0;
}

// write the info in file structure to disk (diritem)
void fatfs_close(file_t *file) {
    if (file->mode == O_RDONLY) {
        return;
    }

    fat_t *fat = (fat_t*)file->fs->data;
    diritem_t item;
    read_dir_entry(fat, file->p_index, &item);
    
    item.DIR_FileSize = file->size;
    // this is needed since it is possible that sblk is invalid at first
    item.DIR_FstClusHI = file->sblk >> 16;
    item.DIR_FstClusL0 = file->sblk;
    write_dir_entry(fat, &item, file->p_index);
}

int fatfs_seek(file_t *file, uint32_t offset, int dir) {
    // file->pos <= offset, set file pos as offset
    if (dir != 0) {
        return -1;
    }

    // starts from the starting cluster 
    fat_t *fat = (fat_t*)file->fs->data;
    uint16_t curr_blk = file->sblk;
    int curr_pos = 0;
    int remain = offset;

    while (remain) {
        if (remain >= fat->cluster_byte_size) {
            remain -= fat->cluster_byte_size;
            curr_pos += fat->cluster_byte_size;
            curr_blk = cluster_get_next(fat, curr_blk);
            if (cluster_invalid(curr_blk)) {
                return -1;
            }
            continue;
        }
        curr_pos += remain;
        remain -= remain;
    }

    file->pos = curr_pos;
    file->cblk = curr_blk;
    return 0;
}

int fatfs_stat(file_t *file, struct stat *st) {
    return -1;
}

int fatfs_unlink(fs_t *fs, const char *file_name) {
    fat_t *fat = (fat_t*)fs->data;
    for (int i = 0; i < fat->root_ent_cnt; i++) {
        diritem_t p_item;
        int ret = read_dir_entry(fat, i, &p_item);
        if (ret < 0) {
            break;
        }
        if (p_item.DIR_Name[0] == DIRITEM_NAME_END) {
            break;
        }
        if (p_item.DIR_Name[0] == DIRITEM_NAME_FREE) {
            continue;
        }
        if(diritem_name_match(file_name, &p_item)) {
            int ret = cluster_free_chain(fat, p_item.DIR_FstClusL0);
            if (ret < 0) {
                return ret;
            }
            diritem_t item;
            kernel_memset(&item, 0, sizeof(diritem_t));
            item.DIR_Name[0] = DIRITEM_NAME_FREE;
            write_dir_entry(fat, &item, i);
            return 0;
        }
    }
    
    return -1;    
}

fs_op_t fatfs_op = {
    .mount = fatfs_mount,
    .unmount = fatfs_unmount,
    .open = fatfs_open,
    .close = fatfs_close,
    .read = fatfs_read,
    .write = fatfs_write,
    .seek = fatfs_seek,
    .stat = fatfs_stat,
    .opendir = fatfs_opendir,
    .readdir = fatfs_readdir,
    .closedir = fatfs_closedir,
    .unlink = fatfs_unlink,
};