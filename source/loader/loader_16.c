__asm__(".code16gcc");

#include "loader.h"

static boot_info_t boot_info;

static void show_msg(const char *msg) {
    char c;
    while (c = *msg++) {
        __asm__ __volatile__ (
            "mov $0xe, %%ah\n\t"
            "mov %[ch], %%al\n\t"
            "int $0x10"::[ch]"r"(c)
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

void loader_entry(void) {
    show_msg("...loading...\n\r");
    detect_memory();
    for (;;) {}
}
