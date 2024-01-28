__asm__(".code16gcc");

#include "boot.h"

#define LOADER_START_ADDR 0x8000

void boot_entry(void) {
    ((void (*)(void))LOADER_START_ADDR)(); // jumps to loader code
}

