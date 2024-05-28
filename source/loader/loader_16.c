__asm__(".code16gcc");

#include "loader.h"

boot_info_t boot_info;

static void show_msg(const char *msg) {
    char c;
    while (c = *msg++) {
        __asm__ __volatile__ (
            "mov $0xe, %%ah\n\t" 
            "mov %[ch], %%al\n\t"
            "int $0x10"::[ch]"r"(c) 
            // \n\t is for multiple instructions
            // :output:input
            // names should be the same (ch)
        );
    }
}

static void detect_memory(void) {
    show_msg("detecting memory...\r\n");

    SMAP_entry_t smap_entry;
    uint32_t contID = 0, signature, bytes;
    
    boot_info.ram_region_count = 0;
    for (int i = 0; i < BOOT_RAM_REGION_MAX; i++) {
        __asm__ __volatile__ (
            // D is the address of the struct storing the info
            "int $0x15": "=a"(signature), "=c"(bytes), "=b"(contID)
            : "a"(0xE820), "b"(contID), "c"(24), "d"(0x534D4150), "D"(&smap_entry)
            );

        if (signature != 0x534D4150) {
            show_msg("failed...\r\n");
            return;
        }

        if (bytes > 20 && ((&smap_entry)->ACPI & 0x0001) == 0) {
            continue;
        }

        if ((&smap_entry)->Type == 1) {
            boot_info.ram_region_info[boot_info.ram_region_count].start = (&smap_entry)->BaseL;
            boot_info.ram_region_info[boot_info.ram_region_count].size = (&smap_entry)->LengthL;
            boot_info.ram_region_count++;
        }

        if (contID == 0) {
            break;
        }
    }

    show_msg("detect memory done\r\n");
}

uint16_t gdt_table[][4] = {
    // each descriptor is 64 bits long
    {0, 0 , 0, 0},
    {0xFFFF, 0x0000, 0x9a00, 0x00cf},
    {0xFFFF, 0x0000, 0x9200, 0x00cf},
};

static void enter_protect_mode(void) {
    // clear interrupt
    cli();

    // open the A20 gate
    uint8_t v = inb(0x92);
    outb(0x92, v | 0x2);

    // load gdt table
    lgdt((uint32_t)gdt_table, sizeof(gdt_table));

    // set cr0 rightmost bit to 1
    uint32_t cr0 = read_cr0();
    write_cr0(cr0 | (1 << 0));

    // do a far jump to clear instruction pipeline
    // we then jump to assembly since we need to set segment registers
    far_jump(8, (uint32_t)protect_mode_entry);
}

void loader_entry(void) {
    show_msg("...loading...\n\r");
    detect_memory();
    enter_protect_mode();
    for (;;) {}
}


