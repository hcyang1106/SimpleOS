#include "core/memory.h"
#include "tools/log.h"
#include "tools/klib.h"
#include "cpu/mmu.h"

#define MEM_EXT_START (1024 * 1024)
#define MEM_EBDA_START (0x80000)
#define MEM_TASK_BASE (0x80000000) // above is space for tasks
#define MEM_EXT_END (127 * 1024 * 1024)

static mem_alloc_t mem_alloc;

static pde_t kernel_page_dir[PDE_CNT] __attribute__((aligned(4096)));

// initialize the memory allocator
static void mem_alloc_init(mem_alloc_t *mem_alloc, uint8_t *bitmap_start, uint32_t mem_start, \
uint32_t mem_size, uint32_t page_size) {
    mutex_init(&mem_alloc->mutex);
    bitmap_init(&mem_alloc->bitmap, bitmap_start, mem_size / page_size, 0);
    mem_alloc->start = mem_start;
    mem_alloc->size = mem_size;
    mem_alloc->page_size = page_size;
}

// allocate (page_count) pages
// find the starting address of (page_count) continuous unused pages
static uint32_t _mem_alloc_page(mem_alloc_t *mem_alloc, int page_count) {
    uint32_t addr = 0;
    mutex_lock(&mem_alloc->mutex);

    int page_index = bitmap_alloc_nbits(&mem_alloc->bitmap, 0, page_count);
    if (page_index >= 0) {
        addr = (mem_alloc->start) + mem_alloc->page_size * page_index;
    }

    mutex_unlock(&mem_alloc->mutex);

    return addr;
}

pte_t *find_pte(pde_t *page_dir, uint32_t vstart, int alloc) {
    pde_t *pde = page_dir + pde_index(vstart);
    if (pde->present) {
        return (pte_t *)(pde->phy_pt_addr << 12) + pte_index(vstart);
    } else {
        if (alloc == 0) {
            return (pte_t*)0;
        }
        // create a page table, page table is of size 4096
        uint32_t phy_pt_addr = _mem_alloc_page(&mem_alloc, 1);
        if (phy_pt_addr == 0) {
            return (pte_t *)0;
        }
         // set up pde
        pde->v = phy_pt_addr | PDE_P | PDE_U | PDE_W;
        // important! otherwise there might some present bits might be 1
        // if remove this line next time when we want to insert to a new pte
        // it already has present bit 1
        kernel_memset((uint8_t*)phy_pt_addr, 0, MEM_PAGE_SIZE); 
       
        
        return (pte_t*)phy_pt_addr + pte_index(vstart);
    }
}

// exported version of mem_alloc_page
uint32_t mem_alloc_page(int page_count) {
    return _mem_alloc_page(&mem_alloc, page_count);
}

static void _mem_free_page(mem_alloc_t *mem_alloc, uint32_t start, int page_count) {
    mutex_lock(&mem_alloc->mutex);

    int page_index = (start - mem_alloc->start) / (mem_alloc->page_size);
    bitmap_set_bit(&mem_alloc->bitmap, page_index, page_count, 0);

    mutex_unlock(&mem_alloc->mutex);
}

void mem_free_page(uint32_t addr, int page_count) {
    // _mem_free_page(&mem_alloc, addr, page_count); simply doing this is wrong 
    // because we also have to deal with the mapping already stored in page table (possibly)
    if (addr < MEM_TASK_BASE) {
        _mem_free_page(&mem_alloc, addr, page_count);
    } else {
        pte_t *pte = find_pte((pde_t*)(task_current()->tss.cr3), addr, 0);
        ASSERT(pte && pte->present);
        _mem_free_page(&mem_alloc, addr, page_count);
        pte->v = 0; // this sets present bit
    }

    return;
}

static void show_mem_info(boot_info_t *boot_info) {
    log_printf("mem region:");
    for (int i = 0; i < boot_info->ram_region_count; i++) {
        log_printf("[%d]: 0x%x, 0x%x", i, \
        boot_info->ram_region_info[i].start, \
        boot_info->ram_region_info[i].size);
    }
    log_printf("\n");
}

static uint32_t total_mem_size(boot_info_t *boot_info) {
    uint32_t mem_size = 0;
    for (int i = 0; i < boot_info->ram_region_count; i++) {
        mem_size += boot_info->ram_region_info[i].size;
    }
    return mem_size;
}

// create page table entries
int memory_create_map(pde_t *page_dir, uint32_t vstart, uint32_t pstart, int page_count, uint32_t perm) {
    for (int i = 0; i < page_count; i++) {
        // log_printf("create map: v-0x%x p-0x%x, perm: 0x%x, page count: %d, i=%d", vstart, pstart, perm, page_count, i);
        pte_t *pte = find_pte(page_dir, vstart, 1);
        if (pte == ((pte_t*)0)) {
            log_printf("create pte failed. pte == 0");
            return -1;
        }
        // log_printf("pte addr: 0x%x", (uint32_t)pte);
        ASSERT(pte->present == 0);

        pte->v = pstart | PTE_P | perm;
        
        vstart += MEM_PAGE_SIZE;
        pstart += MEM_PAGE_SIZE;
    }

    return 0; 
}

// set up the page table entries for kernel segments (text, data...)
static void create_kernel_table(void) {
    extern uint8_t s_text, e_text, s_data;
    // we control permission at pte, and we make pde accessible and writable by all 
    static memory_map_t kernel_map[] = {
        {0, &s_text, 0, PTE_W},
        {&s_text, &e_text, &s_text, 0},
        {&s_data, (void*)MEM_EBDA_START - 1, &s_data, PTE_W},
        {(void*)MEM_EXT_START, (void*)MEM_EXT_END - 1, (void*)MEM_EXT_START, PTE_W}
    };

    kernel_memset(kernel_page_dir, 0, sizeof(kernel_page_dir)); // optional because it is placed in bss (zero)

    for (int i = 0; i < sizeof(kernel_map) / sizeof(memory_map_t); i++) {
        memory_map_t *map = kernel_map + i;
        uint32_t vstart = down((uint32_t)map->vstart, MEM_PAGE_SIZE);
        uint32_t vend = up((uint32_t)map->vend, MEM_PAGE_SIZE);
        uint32_t pstart = down((uint32_t)map->pstart, MEM_PAGE_SIZE);

        int page_count = (vend - vstart) / MEM_PAGE_SIZE;

        memory_create_map(kernel_page_dir, vstart, pstart, page_count, map->perm);
    }  
}

// allocate a page directory
// set make lower pdes point to kernel pages (which is already created)
// return page dir addr
uint32_t memory_create_uvm(void) {
    uint32_t pg_dir_addr = _mem_alloc_page(&mem_alloc, 1);
    if (pg_dir_addr == 0) {
        return 0;
    }

    // page directory needs to be initialized
    kernel_memset((void*)pg_dir_addr, 0, MEM_PAGE_SIZE);
    uint32_t user_pde_start = pde_index(MEM_TASK_BASE); // until the start of task code
    pde_t *p = (pde_t*)pg_dir_addr;
    for (int i = 0; i < user_pde_start; i++) {
        p[i].v = kernel_page_dir[i].v; // can use the page tables already created 
    }

    return pg_dir_addr;
}

// allocate page in physical mem and map the phy starting addr with virtual address vstart
// todo: if memory_create_map fails should do mem_free_page
int alloc_mem_for_task(uint32_t page_dir, uint32_t page_count, uint32_t vstart, uint32_t perm) {
    if (!page_dir) {
        log_printf("page dir not found");
        return -1;
    }
    for (int i = 0; i < page_count; i++) {
        uint32_t pstart = _mem_alloc_page(&mem_alloc, 1);
        if (pstart == 0) {
            log_printf("mem alloc page failed");
            return -1;
        }

        int err = memory_create_map((pde_t*)page_dir, vstart, pstart, 1, perm);
        if (err < 0) {
            log_printf("mem create map entries failed");
            return -1;
        }

        vstart += MEM_PAGE_SIZE;
    }

    return 0;
}

void memory_init(boot_info_t *boot_info) {
    // testing for memory apis

    // mem_alloc_t mem_alloc;
    // uint8_t bytes[8];
    // mem_alloc_init(&mem_alloc, bytes, 0x1000, 64*4096, 4096);
    // // alloc pages
    // for (int i = 0; i < 32; i++) {
    //     uint32_t addr = _mem_alloc_page(&mem_alloc, 2);
    //     log_printf("0x%x", addr);
    // }
    // // free pages
    // uint32_t start = 0x1000;
    // for (int i = 0; i < 32; i++) {
    //     mem_free_page(&mem_alloc, start + 4096*2*i, 2);
    // }

    // not sure why we do it like this, code refers to this:
    // https://stackoverflow.com/questions/39998485/how-to-access-variable-define-in-linker-script-in-c
    extern uint8_t mem_free_start; // the address pointing to can not be modified!
    uint8_t *mem_free = &mem_free_start;

    log_printf("mem init");
    show_mem_info(boot_info);
    // approx value, we put os in 1MB and applications above 1MB
    uint32_t free_mem_above_1MB = total_mem_size(boot_info) - MEM_EXT_START; 
    free_mem_above_1MB = down(free_mem_above_1MB, MEM_PAGE_SIZE);

    mem_alloc_init(&mem_alloc, mem_free, MEM_EXT_START, free_mem_above_1MB, MEM_PAGE_SIZE);
    mem_free += bitmap_byte_count(mem_alloc.size / MEM_PAGE_SIZE);

    ASSERT(mem_free < (uint8_t*)MEM_EBDA_START);

    create_kernel_table();

    mmu_set_page_dir((uint32_t)kernel_page_dir);
}