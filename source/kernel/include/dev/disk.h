#ifndef DISK_H
#define DISK_H

#include "comm/types.h"
#include "ipc/mutex.h"
#include "ipc/sem.h"
#include "tools/buffer.h"

#define PART_NAME_SIZE 32
#define DISK_NAME_SIZE 32
#define DISK_PRIM_PART_NUM (4+1)
#define DISK_NUM 2
#define DISK_PER_CHANNEL 2 // primary bus, master and slave
#define IOBASE_PRIMARY 0x1F0

#define DISK_DATA(disk) (disk->port_base + 0)
#define DISK_ERROR(disk) (disk->port_base + 1)
#define DISK_SECTOR_COUNT(disk) (disk->port_base + 2)
#define DISK_LBA_LO(disk) (disk->port_base + 3)
#define DISK_LBA_MID(disk) (disk->port_base + 4)
#define DISK_LBA_HI(disk) (disk->port_base + 5)
#define DISK_DRIVE(disk) (disk->port_base + 6)
#define DISK_STATUS(disk) (disk->port_base + 7)
#define DISK_CMD(disk) (disk->port_base + 7)

#define DISK_CMD_IDENTIFY 0xEC
#define DISK_CMD_READ 0x24
#define DISK_CMD_WRITE 0x34

#define DISK_STATUS_ERR (1 << 0)
#define DISK_STATUS_DRQ (1 << 3)
#define DISK_STATUS_DF (1 << 5)
#define DISK_STATUS_BUSY (1 << 7)

#define	DISK_DRIVE_BASE 0xE0 // first three bits (from left) are zeros

#define MBR_PRIM_PART_NUM 4

#define DISK_OBUF_SIZE 1024

#pragma pack(1) // has to be 512 bytes
typedef struct _part_item_t {
    uint8_t boot_active;               
	uint8_t start_header;          
	uint16_t start_sector : 6;         
	uint16_t start_cylinder : 10;	    
	uint8_t system_id;	                
	uint8_t end_header;                
	uint16_t end_sector : 6;           
	uint16_t end_cylinder : 10;        
	uint32_t relative_sectors; // for LBA      
	uint32_t total_sectors;
}part_item_t;

typedef struct _mbr_t {
    uint8_t code[446];
    part_item_t part_item[MBR_PRIM_PART_NUM];
    uint8_t boot_sig[2];
}mbr_t;
#pragma pack(1)

struct _disk_t;

typedef struct _partinfo_t {
    char name[PART_NAME_SIZE];
    struct _disk_t *disk;
    enum {
        FS_INVALID = 0x00,
        FS_FAT16_0 = 0x6,
        FS_FAT16_1 = 0xE,
    }type;
    int start_sector;
    int total_sector;
}partinfo_t;

typedef struct _disk_t {
    char name[DISK_NAME_SIZE];
    enum {
        DISK_MASTER = (0 << 4),
        DISK_SLAVE = (1 << 4),
    }drive;
    uint16_t port_base;
    int sector_size;
    int sector_count;
    partinfo_t partinfo[DISK_PRIM_PART_NUM];
    mutex_t *mutex;
    sem_t *osem_full; // not used anymore
    sem_t *isem;
    sem_t *osem; // isem and osem can be combined
    fifo_t *ofifo; // not used anymore
}disk_t;

void disk_init(void);
void exception_handler_disk_primary(void);

#endif