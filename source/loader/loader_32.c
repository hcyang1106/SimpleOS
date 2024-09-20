#include "loader.h"
#include "comm/elf.h"
#include "os_cfg.h"

static void read_disk(uint32_t sector, uint32_t sector_count, uint8_t *buf) {
    outb(0x1F6, 0xE0); // fifth and seventh bit should be set to 1 (fixed usage), sixth bit is set to 1 to choose LBA mode
    outb(0x1F2, (uint8_t)(sector_count >> 8)); // first byte (from right)
    outb(0x1F3, (uint8_t)sector >> 24); // first byte (from right)
    outb(0x1F4, 0);
    outb(0x1F5, 0);

    outb(0x1F2, (uint8_t)sector_count); // second byte (from right)
    outb(0x1F3, (uint8_t)sector); // last byte (from right)
    outb(0x1F4, (uint8_t)(sector >> 8)); // third byte (from right)
    outb(0x1F5, (uint8_t)(sector >> 16)); // second byte (from right)

    outb(0x1F7, 0x24); // Send the "READ SECTORS EXT" command (0x24) to port 0x1F7

    uint16_t *data_buf = (uint16_t *)buf; // it reads 2 bytes at a time
    while (sector_count--) {
        while ((inb(0x1F7) & 0x88) != 0x8) {}// check if DRQ is one and BSY is zero
        for (int i = 0; i < SECTOR_SIZE / 2; i++) { 
            *data_buf++ = inw(0x1F0); // read 2 bytes at a time
        }
    }
}

static uint32_t reload_elf_file(uint8_t *file_buffer) {
    Elf32_Ehdr *elf_hdr = (Elf32_Ehdr *)file_buffer;
    
    if ((elf_hdr->e_ident[0] != 0x7f) || (elf_hdr->e_ident[1] != 'E')
        || (elf_hdr->e_ident[2] != 'L') || (elf_hdr->e_ident[3] != 'F')) {
            return 0;
    }

    for (int i = 0; i < elf_hdr->e_phnum; i++) {
        Elf32_Phdr *phdr = (Elf32_Phdr *)(file_buffer + elf_hdr->e_phoff) + i; // e_phoff is the offset of the program head table
        if (phdr->p_type != PT_LOAD) { // check if is loadable
            continue;
        }

        uint8_t *src = file_buffer + phdr->p_offset;
        uint8_t *dest = (uint8_t *)phdr->p_paddr;

        for (int j = 0; j < phdr->p_filesz; j++) {
            *dest++ = *src++;
        }

        // bss data would not be stored inside elf file. (since they are not initialized we don't need extra space to store them)
        // Elf file uses memsz to specify how large bss and rodata combined is.
        // dest = (uint8_t *)phdr->p_paddr + phdr->p_filesz;
        for (int j = 0; j < phdr->p_memsz - phdr->p_filesz; j++) {
            // set as zeros and this is why bss area are zeros
            *dest++ = 0;
        }
    }
    
    return elf_hdr->e_entry; // return the entry of kernel
}

static void die (int code) {
    for (;;) {}
}

#define CR4_PSE (1 << 4) // 4MB page
#define CR0_PG (1 << 31) // turns on page table mode
#define PDE_P (1 << 0) // descriptor present or not
#define PDE_W (1 << 1) // write enable
#define PDE_PS (1 << 7) // 4MB page

// this is just an experiment for 4MB page and is just for loader
// later in kernel init will use another page table
void enable_page_mode(void) {
    static uint32_t page_dir[1024] __attribute__((aligned(4096))) = {
        [0] = PDE_P | PDE_W | PDE_PS | 0 // map virtual exactly to physical
    };

    // only if this is set we can use 4MB page
    uint32_t cr4 = read_cr4();
    write_cr4(cr4 | CR4_PSE);

    // the manual asks cr3 last 12 bits to be zero, so we make page_dir align to 4096
    // cr3 sets as page dir address
    write_cr3((uint32_t)(page_dir));

    write_cr0(read_cr0() | CR0_PG);
}

void load_kernel(void) {
    read_disk(100, 500, (uint8_t *)SYS_KERNEL_LOAD_ADDR); // kernel code is written to 100th sector using script
    uint32_t kernel_entry = reload_elf_file((uint8_t *)SYS_KERNEL_LOAD_ADDR);
    if (kernel_entry == 0) {
        die(-1);
    }

    enable_page_mode(); // page mode is turned on before jumping to kernel entry

    // kernel is not in the same folder so need to jump to assembly again (same as boot)
    // ((void (*)(boot_info_t *))0x10000)(&boot_info); // jumps to kernel code,
    // but kernel entry is not guaranteed to be 0x10000 so use the line below
    // * when calling a function the caller will push parameters in caller (assembly)
    ((void (*)(boot_info_t *))kernel_entry)(&boot_info); // should use the entry the elf file specifies
    for (;;) {}
}

// & operator returns the "pointer" of the operand (&int => type of int*)
