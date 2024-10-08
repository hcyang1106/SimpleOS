#ifndef MEMORY_H
#define MEMORY_H

#include "tools/bitmap.h"
#include "comm/types.h"
#include "ipc/mutex.h"
#include "comm/boot_info.h"

#define PDE_CNT 1024
#define PTE_CNT 1024
#define MEM_PAGE_SIZE (4096)
#define MEM_TASK_BASE (0x80000000) // above is space for tasks

#define MEM_TASK_STACK_TOP 0xE0000000
#define MEM_TASK_STACK_SIZE (MEM_PAGE_SIZE * 500)
#define MEM_TASK_ARG_SIZE (MEM_PAGE_SIZE * 4)

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
uint32_t mem_alloc_page(int page_count);
void mem_free_page(uint32_t addr, int page_count);
uint32_t memory_copy_uvm(uint32_t page_dir);
void memory_destroy_uvm(uint32_t page_dir);
uint32_t memory_get_paddr(uint32_t page_dir, uint32_t vaddr);
int memory_copy_uvm_data(uint32_t to, uint32_t page_dir, uint32_t from, uint32_t size);
char *sys_sbrk(int incr);

#endif