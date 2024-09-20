#ifndef MMU_H
#define MMU_H

#include "comm/types.h"
#include "comm/cpu_instr.h"

#define PTE_P (1 << 0)
#define PDE_P (1 << 0)
#define PTE_W (1 << 1)
#define PDE_U (1 << 2)
#define PDE_W (1 << 1)
#define PTE_U (1 << 2)

// useful links for union:
// basically it is used to save memory
// https://stackoverflow.com/questions/252552/why-do-we-need-c-unions
// https://stackoverflow.com/questions/7950689/when-to-use-a-union-and-when-to-use-a-structure
typedef union _pde_t {
    uint32_t v; // the bits of the entry
    struct { // for bit operations we usually use unsigned int (uint32_t)
        uint32_t present: 1;
        uint32_t write_enable: 1;
        uint32_t user_mode_access: 1;
        uint32_t write_through: 1;
        uint32_t cache_disable: 1;
        uint32_t accessed: 1;
        uint32_t : 1;
        uint32_t ps : 1;
        uint32_t : 4;
        // page table should also be aligned (4096), so the lower 12 bits are all zeros (32-12=20)
        uint32_t phy_pt_addr: 20;
    };
}pde_t;

typedef union _pte_t {
    uint32_t v; // the bits of the entry
    struct { // for bit operations we usually use unsigned int (uint32_t)
        uint32_t present: 1;
        uint32_t write_enable: 1;
        uint32_t user_mode_access: 1;
        uint32_t write_through: 1;
        uint32_t cache_disable: 1;
        uint32_t accessed: 1;
        uint32_t dirty: 1;
        uint32_t pat : 1;
        uint32_t global: 1;
        uint32_t : 3;
        // page is also aligned (4096), so the lower 12 bits are all zeros (32-12=20)
        uint32_t phy_page_addr: 20;
    };
}pte_t;

static inline uint32_t pde_index(uint32_t vaddr) {
    return vaddr >> 22;
}

static inline uint32_t pte_index(uint32_t vaddr) {
    return (vaddr >> 12) & 0x3FF;
}

static inline void mmu_set_page_dir(uint32_t page_dir) {
    write_cr3(page_dir);
}

static inline uint32_t get_pte_perm(pte_t *pte) {
    return (pte->v & 0x1FF);     
}

static inline uint32_t pde_paddr (pde_t * pde) {
    return pde->phy_pt_addr << 12;
}

static inline uint32_t pte_paddr (pte_t * pte) {
    return pte->phy_page_addr << 12;
}

#endif


