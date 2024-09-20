#ifndef FATFS_H
#define FATFS_H

#define FAT_CLUSTER_INVALID 0xFFF8
#define FAT_CLUSTER_FREE 0x00

#define DIRITEM_NAME_FREE 0xE5
#define DIRITEM_NAME_END 0x00

#define DIRITEM_ATTR_READ_ONLY 0x01
#define DIRITEM_ATTR_HIDDEN 0x02
#define DIRITEM_ATTR_SYSTEM 0x04
#define DIRITEM_ATTR_VOLUME_ID 0x08
#define DIRITEM_ATTR_DIRECTORY 0x10
#define DIRITEM_ATTR_ARCHIVE 0x20
#define DIRITEM_ATTR_LONG_NAME 0x0F

#define FORMAT_NAME_LEN 11

#pragma pack(1)

typedef struct _diritem_t {
    uint8_t DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t DIR_NTRes;
    uint8_t DIR_CrtTimeTeenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LastAccDate;
    uint16_t DIR_FstClusHI; // first cluster high 16 bits
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusL0; // first cluster low 16 bits
    uint32_t DIR_FileSize;
}diritem_t;

typedef struct _dbr_t {
    uint8_t BS_jmpBoot[3];
    uint8_t BS_OEMName[8];
    uint16_t BPB_BytsPerSec;
    uint8_t BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt; // sectors before fat tables
    uint8_t BPB_NumFATs;     // number of fat tables
    uint16_t BPB_RootEntCnt; // number of entries in root area
    uint16_t BPB_TotSec16;   // total sectors
    uint8_t BPB_Media;
    uint16_t BPB_FATSz16;    // fat entry size
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;

	uint8_t BS_DrvNum;
	uint8_t BS_Reserved1;
	uint8_t BS_BootSig;
	uint32_t BS_VolID;
	uint8_t BS_VolLab[11];
    // this fills in the type of file system (fat16, fat32...)
    // but does not imply its real type
	uint8_t BS_FileSysType[8];
} dbr_t;
#pragma pack()

typedef struct _fat_t {
    uint32_t tbl_start;                     
    uint32_t tbl_cnt;                       
    uint32_t tbl_sectors;
    uint32_t bytes_per_sec;
    uint32_t sec_per_cluster;
    uint32_t root_ent_cnt;
    uint32_t root_start;
    uint32_t data_start;
    uint32_t cluster_byte_size;
    uint8_t *fat_buffer;
    uint32_t sector_idx; // absolute sector index
    struct _fs_t *fs;
} fat_t;

#endif