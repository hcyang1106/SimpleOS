#include "dev/disk.h"
#include "tools/log.h"
#include "tools/klib.h"
#include "comm/cpu_instr.h"
#include "os_cfg.h"
#include "dev/dev.h"
#include "cpu/cpu.h"
#include "ipc/mutex.h"
#include "ipc/sem.h"

static disk_t disk_buf[DISK_NUM];
static mutex_t disk_mutex;
static char disk_obuf[DISK_OBUF_SIZE];
static fifo_t disk_ofifo;
// static sem_t disk_osem_full; not used anymore
static sem_t disk_isem;
static disk_t *curr_disk;
static sem_t disk_osem; // disk_isem and disk_osem can be combined (use only one sem)

static enum {
    DISK_STATE_READ = 0,
    DISK_STATE_WRITE,
    DISK_STATE_UNKNOWN,
}state = DISK_STATE_UNKNOWN;


static void disk_send_cmd(disk_t *disk, uint32_t start_sector, uint32_t sector_count, int cmd) {
    outb(DISK_DRIVE(disk), DISK_DRIVE_BASE | disk->drive);		

	outb(DISK_SECTOR_COUNT(disk), (uint8_t) (sector_count >> 8));
	outb(DISK_LBA_LO(disk), (uint8_t) (start_sector >> 24));	
	outb(DISK_LBA_MID(disk), 0);								
	outb(DISK_LBA_HI(disk), 0);									
	outb(DISK_SECTOR_COUNT(disk), (uint8_t) (sector_count));	
	outb(DISK_LBA_LO(disk), (uint8_t) (start_sector >> 0));		
	outb(DISK_LBA_MID(disk), (uint8_t) (start_sector >> 8));	
	outb(DISK_LBA_HI(disk), (uint8_t) (start_sector >> 16));

	outb(DISK_CMD(disk), (uint8_t)cmd);
}

static int disk_wait_data(disk_t *disk) {
    uint8_t status;

    do {
        status = inb(DISK_STATUS(disk));
    }while((status & (DISK_STATUS_BUSY | DISK_STATUS_DRQ | DISK_STATUS_ERR)) == DISK_STATUS_BUSY);

    if (status & DISK_STATUS_ERR) {
        return -1;
    }

    return 0;
}

static inline void disk_read_data (disk_t *disk, void *buf, int size) {
    uint16_t *c = (uint16_t*)buf;
    for (int i = 0; i < size / 2; i++) {
        *c++ = inw(DISK_DATA(disk));
    }
}

static inline void disk_write_data (disk_t *disk, void *buf, int size) {
    uint16_t *c = (uint16_t*)buf;
    for (int i = 0; i < size / 2; i++) {
        outw(DISK_DATA(disk), *c++);
    }
}

static int detect_part_info(disk_t *disk) {
    mbr_t mbr;
    disk_send_cmd(disk, 0, 1, DISK_CMD_READ);
    int err = disk_wait_data(disk);
    if (err < 0) {
        log_printf("read part info failed, name=%s", disk->name);
        return err;
    }

    disk_read_data(disk, &mbr, sizeof(mbr_t)); // read into mbr structure
    
    // use info in mbr to fill in partinfo in disk
    partinfo_t *dest = disk->partinfo + 1; // index 0 is for the whole disk
    part_item_t *src = mbr.part_item;
    for (int i = 0; i < MBR_PRIM_PART_NUM; i++, dest++, src++) {
        if (src->system_id == FS_INVALID) {
            dest->type = src->system_id;
			dest->total_sector = 0;
            dest->start_sector = 0;
            dest->disk = (disk_t *)0;
            continue;
        }
        dest->disk = disk;
        kernel_sprintf(dest->name, "%s%d", disk->name, i+1);
        dest->start_sector = src->relative_sectors;
        dest->total_sector = src->total_sectors;
        dest->type = src->system_id; 
    }

    return 0;
}

static int identify_disk(disk_t *disk) {
    disk_send_cmd(disk, 0, 0, DISK_CMD_IDENTIFY); // start_sector and sector_count required to be 0

    int err = inb(DISK_STATUS(disk));
    if (!err) {
        log_printf("disk does not exist, name=%s", disk->name);
        return -1;
    }

    int ret = disk_wait_data(disk);
    if (ret < 0) {
        log_printf("disk read failed, name=%s", disk->name);
        return ret;
    }

    uint16_t buf[256];
    disk_read_data(disk, buf, sizeof(buf));
    disk->sector_size = SECTOR_SIZE;
    disk->sector_count = *(uint32_t*)(buf + 100);

    // sda, sdb; sda0, sda1 => view the whole disk as sda0
    partinfo_t *part = disk->partinfo + 0;
    part->disk = disk;
    kernel_sprintf(part->name, "%s%d", disk->name, 0);
    part->start_sector = 0;
    part->total_sector = disk->sector_count;
    part->type = FS_INVALID;

    ret = detect_part_info(disk);
    if (ret < 0) {
        log_printf("detect partitions failed, name=%s", disk->name);
        return ret;
    }

    return 0;
}

static void print_disk_info(disk_t *disk) {
    log_printf("%s:", disk->name);
    log_printf("    port_base: %x", disk->port_base);
    log_printf("    total_size: %dM", disk->sector_count * disk->sector_size / 1024 /1024);
    log_printf("    drive: %s", disk->drive == DISK_MASTER ? "Master" : "Slave");

    log_printf("    part info:");
    for (int i = 0; i < DISK_PRIM_PART_NUM; i++) {
        partinfo_t * part_info = disk->partinfo + i;
        if (part_info->type != FS_INVALID) {
            log_printf("        %s: type: %x, start sector: %d, count %d",
                    part_info->name, part_info->type,
                    part_info->start_sector, part_info->total_sector);
        }
    }
}

void disk_init(void) {
    log_printf("checking disk...");
    mutex_init(&disk_mutex);
    kernel_memset(disk_buf, 0, sizeof(disk_buf));
    kernel_memset(disk_obuf, 0, sizeof(disk_obuf));
    fifo_init(&disk_ofifo, disk_obuf, DISK_OBUF_SIZE);
    sem_init(&disk_isem, 0);
    sem_init(&disk_osem, 0);
    // sem_init(&disk_osem_full, DISK_OBUF_SIZE / SECTOR_SIZE);
    for (int i = 0; i < DISK_PER_CHANNEL; i++) {
        disk_t *disk = disk_buf + i;
        kernel_sprintf(disk->name, "sd%c", i + 'a');
        disk->drive = (i == 0) ? DISK_MASTER : DISK_SLAVE;
        disk->port_base = IOBASE_PRIMARY;
        disk->mutex = &disk_mutex;
        disk->isem = &disk_isem;
        // disk->osem_full = &disk_osem_full;
        disk->ofifo = &disk_ofifo;
        disk->osem = &disk_osem; // 加的

        int err = identify_disk(disk);
        if (err == 0) {
            print_disk_info(disk);
        }
        ASSERT(err == 0);
    }
}

int disk_open(device_t *dev) {
    int disk_idx = (dev->minor >> 4) - 0xa;
    if (disk_idx < 0 || disk_idx >= DISK_NUM) {
        log_printf("invalid disk index, disk idx=%d", disk_idx);
        return -1;
    }

    disk_t *disk = disk_buf + disk_idx;
    if (disk->sector_count == 0) {
        log_printf("disk with zero sector count, disk idx=%d", disk_idx);
        return -1;
    }

    int part_idx = dev->minor & 0xF;
    if (part_idx < 0 || part_idx >= DISK_PRIM_PART_NUM) {
        log_printf("invalid part index, disk idx=%d, part idx=%d", disk_idx, part_idx);
        return -1;
    } 

    partinfo_t *part = disk->partinfo + part_idx;
    if (part->total_sector == 0) {
        log_printf("part with zero sector count, disk idx=%d, part idx=%d", disk_idx, part_idx);
    }

    dev->data = part; // save in data field so that there's no need to repeat code again

    irq_install(IRQ14_DISK_PRIMARY, (irq_handler_t)exception_handler_disk_primary);
    irq_enable(IRQ14_DISK_PRIMARY);
    return 0;
}

int disk_read(device_t *dev, int start_sector, char *buf, int count) {
    partinfo_t *part = (partinfo_t *)dev->data; // part is saved in dev in disk_open
    if (!part) {
        log_printf("get partition failed, device=%d", dev->minor);
        return -1;
    }

    disk_t *disk = part->disk;
    if (disk == (disk_t *)0) {
        log_printf("null disk, device=%d", dev->minor);
        return -1;
    }

    mutex_lock(disk->mutex);
    curr_disk = disk;
    state = DISK_STATE_READ;

    disk_send_cmd(disk, part->start_sector + start_sector, count, DISK_CMD_READ);
    for (int i = 0; i < count; i++) {
        if (task_current()) { // can't call sem_wait before os is prepared
            sem_wait(disk->isem);
        } 
        int ret = disk_wait_data(disk);
        if (ret < 0) {
            log_printf("disk error during wait, device=%d", dev->minor);
            mutex_unlock(disk->mutex);
            return -1;
        }

        disk_read_data(disk, buf, SECTOR_SIZE);
        buf += SECTOR_SIZE;
    }

    mutex_unlock(disk->mutex);
    return count;
}

int disk_write(device_t *dev, int start_sector, char *buf, int count) {
    partinfo_t *part = (partinfo_t *)dev->data; // part is saved in dev in disk_open
    if (!part) {
        log_printf("get partition failed, device=%d", dev->minor);
        return -1;
    }

    disk_t *disk = part->disk;
    if (disk == (disk_t *)0) {
        log_printf("null disk, device %d", dev->minor);
        return -1;
    }

    mutex_lock(disk->mutex);
    curr_disk = disk;
    state = DISK_STATE_WRITE;

    disk_send_cmd(disk, part->start_sector + start_sector, count, DISK_CMD_WRITE);

    for (int i = 0; i < count; i++) {
        disk_write_data(disk, buf, disk->sector_size);

        if (task_current()) {
            sem_wait(disk->osem);
        }

        int err = disk_wait_data(disk);
        if (err < 0) {
            log_printf("disk(%s) write error: start sect %d, count %d", disk->name, start_sector, count);
            break;
        }
    }

    // if (disk->ofifo->count == 0) {
    //     char *curr = buf;
    //     if (count > 1) {
    //         curr += SECTOR_SIZE;
    //         for (int i = 0; i < count - 1; i++) {
    //             if (task_current()) {
    //                 sem_wait(disk->osem_full);
    //             }
    //             fifo_put_sector_size(disk->ofifo, curr);
    //             curr += SECTOR_SIZE;
    //             // sem_notify(disk->osem_empty); // notify by interrupt
    //         }
    //     }

    //     int ret = disk_wait_data(disk);
    //     if (ret < 0) {
    //         log_printf("disk error occurred, device=%d", dev->minor);
    //         return -1;
    //     }
    //     disk_write_data(disk, buf, SECTOR_SIZE);
    //     curr += SECTOR_SIZE;
    // } else {
    //     for (int i = 0; i < count; i++) {
    //         if (task_current()) {
    //             sem_wait(disk->osem_full);
    //         }
    //         fifo_put_sector_size(disk->ofifo, buf);
    //         buf += SECTOR_SIZE;
    //         // sem_notify(disk->osem_empty); // notify by interrupt
    //     }
    // }
    mutex_unlock(disk->mutex);
    return count;
}

int disk_control(device_t *dev, int cmd, int arg0, int arg1) {

}

int disk_close(device_t *dev) {

}

void do_handler_disk_primary(exception_frame_t *frame) {
    pic_send_eoi(IRQ14_DISK_PRIMARY);
    switch (state)
    {
    case DISK_STATE_WRITE:
        // char *curr;
        // if (fifo_get_sector_size(curr_disk->ofifo, &curr) < 0) {
        //     mutex_unlock(curr_disk->mutex);
        //     return;
        // }

        // disk_wait_data(curr_disk);
        // disk_write_data(curr_disk, curr, SECTOR_SIZE);
        // if (task_current()) {
        //     sem_notify(curr_disk->osem_full);
        // }
        if (task_current()) {
            sem_notify(curr_disk->osem);
        }
        break;
    case DISK_STATE_READ:
        if (task_current()) {
            sem_notify(curr_disk->isem);
        }
        break;
    default:
        break;
    }
}

dev_desc_t dev_disk_desc = {
    .name = "disk",
    .major = DEV_DISK,
    .open = disk_open,
    .read = disk_read,
    .write = disk_write,
    .control = disk_control,
    .close = disk_close,
};