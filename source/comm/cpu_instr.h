#ifndef CPU_INSTR_H
#define CPU_INSTR_H

#include "types.h"

static inline void cli(void) { // clear interrupt
    __asm__ __volatile__("cli");
}

static inline void sti(void) { // set interrupt
    __asm__ __volatile__("sti");
}

static inline uint8_t inb(uint16_t port) { // read a byte from a specific port
    uint8_t rv;
    __asm__ __volatile__("inb %[p], %[v]" : [v]"=a"(rv) : [p]"d"(port));

    return rv;
}

static inline uint16_t inw(uint16_t port) { // read a word from a specific port 
    uint16_t rv;
    __asm__ __volatile__("in %[p], %[v]" : [v]"=a"(rv) : [p]"d"(port));

    return rv;
}

static inline uint8_t outb(uint16_t port, uint8_t data) { // writes a byte to a specific port
    __asm__ __volatile__("outb %[v], %[p]" :: [p]"d"(port), [v]"a"(data));
}

static inline void lgdt(uint32_t start, uint32_t size) {
    struct {
        uint16_t limit;
        uint16_t start15_0;
        uint16_t start31_16;
    }gdt;

    gdt.start31_16 = start >> 16;
    gdt.start15_0 = start & 0xFFFF;
    gdt.limit = size - 1;

    __asm__ __volatile__("lgdt %[g]"::[g]"m"(gdt)); // m means a memory address
}

static inline uint32_t read_cr0(void) {
    uint32_t cr0;
    __asm__ __volatile__ ("mov %%cr0, %[v]":[v]"=r"(cr0));
    return cr0;
}

static inline void write_cr0(uint32_t v) {
    __asm__ __volatile__ ("mov %[v], %%cr0"::[v]"r"(v));
}

static inline void far_jump(uint32_t selector, uint32_t offset) {
    uint32_t addr[] = {offset, selector};
     __asm__ __volatile__ ("ljmpl *(%[a])"::[a]"r"(addr)); // * indicates that we are using an address 
}

#endif
