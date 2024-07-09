#ifndef MEMORY_H
#define MEMORY_H

#include "tools/bitmap.h"
#include "comm/types.h"
#include "ipc/mutex.h"
#include "comm/boot_info.h"

#define PDE_CNT 1024

typedef struct {
    mutex_t mutex;
    bitmap_t bitmap;
    uint32_t start; // the start address managed by allocator
    uint32_t size; // the size of memory managed by allocator
    uint32_t page_size;
}mem_alloc_t;

typedef struct {
    void *vstart;
    void *vend;
    void *pstart;
    uint32_t perm; // permission of the segment
}memory_map_t;

void memory_init(boot_info_t *boot_info);
uint32_t memory_create_uvm(void);
int alloc_mem_for_task(uint32_t page_dir, uint32_t page_count, uint32_t vstart, uint32_t perm);

#endif