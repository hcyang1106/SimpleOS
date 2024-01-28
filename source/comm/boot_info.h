#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#define BOOT_RAM_REGION_MAX 10

#include "types.h"

typedef struct _boot_info_t {
    struct {
        uint32_t start;
        uint32_t size;
    }ram_region_info[BOOT_RAM_REGION_MAX];

    int ram_region_count;
}boot_info_t;

typedef struct _SMAP_entry_t {
    uint32_t BaseL;
    uint32_t BaseH;
    uint32_t LengthL;
    uint32_t LengthH;
    uint32_t Type;
    uint32_t ACPI;
} __attribute__((packed)) SMAP_entry_t;

#endif

