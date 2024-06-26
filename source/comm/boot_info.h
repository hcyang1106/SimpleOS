#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#define BOOT_RAM_REGION_MAX 10
#define SECTOR_SIZE 512
// kernel is put at memory address above 1MB (where protected mode could reach)
#define SYS_KERNEL_LOAD_ADDR (1024 * 1024)

#include "types.h"

typedef struct _boot_info_t {
    struct {
        uint32_t start;
        uint32_t size;
    }ram_region_info[BOOT_RAM_REGION_MAX];

    int ram_region_count;
}boot_info_t;

// struct used to save the info of available memory
// packed keyword ask compiler not to do padding (make it not aligned)
// usually struct is aligned to have faster accessing
// cpu access memory using the unit of "word", so if not aligned it may need "more than one memory access" to get data
typedef struct _SMAP_entry_t {
    uint32_t BaseL;
    uint32_t BaseH;
    uint32_t LengthL;
    uint32_t LengthH;
    uint32_t Type;
    uint32_t ACPI;
} __attribute__((packed)) SMAP_entry_t;


// example of struct alignment

// struct test1
// {
//     short s; 
//     // 2 bytes
//     // 2 padding bytes
//     int i;
//     // 4 bytes
//     char c;
//     // 1 byte
//     // 3 padding bytes
// };

// struct test2
// {
//     int i;
//     // 4 bytes
//     char c;
//     // 1 byte
//     // 1 padding byte
//     short s;
//     // 2 bytes
// };


#endif

