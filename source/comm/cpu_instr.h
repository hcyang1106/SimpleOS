#ifndef CPU_INSTR_H
#define CPU_INSTR_H

#include "types.h"

// inline copies code directly into the code of calling function, which reduces overhead
// useful link 1: https://tonybai.com/2011/06/22/also-talk-about-inline-function-in-c/
// useful link 2: https://stackoverflow.com/questions/62374711/c-inline-function-generates-undefined-symbols-error
// useful link 3: https://wdv4758h-notes.readthedocs.io/zh-tw/latest/c/internal_and_external_linkage_in_C.html
// useful link for "volatile" keyword: https://stackoverflow.com/questions/18695120/volatile-and-cache-behaviour
// adding volatile keyword prevents compiler from doing optimization (changing the order of instructions, caching value of registers)

static inline void cli(void) { // clear interrupt
    __asm__ __volatile__("cli");
}

static inline void sti(void) { // set interrupt
    __asm__ __volatile__("sti");
}

// output reg needs a "="
// read a byte from a specific port
static inline uint8_t inb(uint16_t port) { 
    uint8_t rv;
    __asm__ __volatile__("inb %[p], %[v]" : [v]"=a"(rv) : [p]"d"(port));

    return rv;
}

// read a word from a specific port 
static inline uint16_t inw(uint16_t port) { 
    uint16_t rv;
    __asm__ __volatile__("in %[p], %[v]" : [v]"=a"(rv) : [p]"d"(port));

    return rv;
}

// writes a byte to a specific port
static inline void outb(uint16_t port, uint8_t data) { 
    __asm__ __volatile__("outb %[v], %[p]" :: [p]"d"(port), [v]"a"(data));
}

// useful link for usage of m: https://juejin.cn/post/6991364336316842014
static inline void lgdt(uint32_t start, uint32_t size) {
    struct {
        uint16_t limit;
        uint16_t start15_0;
        uint16_t start31_16;
    }gdt;

    gdt.start31_16 = start >> 16;
    gdt.start15_0 = start & 0xFFFF;
    gdt.limit = size - 1;

    __asm__ __volatile__("lgdt %[g]"::[g]"m"(gdt)); // m means directly from memory (use the contents in memory directly)
    // __asm__ __volatile__("lgdt %[g]"::[g]"r"(&gdt)); doesn't work since &gdt is 32 bits but we need 48 bits?
}

static inline void lidt(uint32_t start, uint32_t size) {
    struct {
        uint16_t limit;
        uint16_t start15_0;
        uint16_t start31_16;
    }idt;

    idt.start31_16 = start >> 16;
    idt.start15_0 = start & 0xFFFF;
    idt.limit = size - 1;

    __asm__ __volatile__("lidt %[i]"::[i]"m"(idt)); // m means directly from memory (use the contents in memory directly)
}

static inline uint32_t read_cr0(void) {
    uint32_t cr0;
    __asm__ __volatile__ ("mov %%cr0, %[v]":[v]"=r"(cr0));
    return cr0;
}

static inline void write_cr0(uint32_t v) {
    // one % is used for variable in c, two % is used for register in assembly
    __asm__ __volatile__ ("mov %[v], %%cr0"::[v]"r"(v));
}

static inline void far_jump(uint32_t selector, uint32_t offset) {
    uint32_t addr[] = {offset, selector};
    __asm__ __volatile__ ("ljmpl *(%[a])"::[a]"r"(addr)); // * indicates that we are dereference from an address
    // __asm__ __volatile__ ("ljmpl (%[a])"::[a]"m"(addr));
}

static inline void hlt(void) {
    __asm__ __volatile__("hlt");
}

static inline void write_tr (uint16_t tss_sel) {
    __asm__ __volatile__("ltr %%ax"::"a"(tss_sel));
}

#endif



// volatile example:

// int main() {
//     (volatile) int a = 10;
//     (volatile) int b = 20;
//     int c = 0;

//     for (int i = 0; i < 1000000; i++) {
//         c = a + b;
//     }

//     return 0;
// }

// mov eax, 10   ; Load value of 'a' into register eax
// mov ebx, 20   ; Load value of 'b' into register ebx
// xor ecx, ecx  ; Initialize 'c' to 0 in register ecx
// mov edx, 1000000 ; Load loop count into register edx

// loop:
//     mov esi, eax  ; Copy value of 'a' from eax into esi
//     add esi, ebx  ; Add value of 'b' (in ebx) to esi (esi = a + b)
//     mov ecx, esi  ; Store the sum (a + b) in 'c' (ecx)
//     dec edx       ; Decrement loop counter
//     jnz loop      ; Jump back to loop if not zero
//     ; ecx now holds the final value of 'c'

// mov eax, 10   ; Load initial value of 'a' into register eax
// mov ebx, 20   ; Load initial value of 'b' into register ebx
// xor ecx, ecx  ; Initialize 'c' to 0 in register ecx
// mov edx, 1000000 ; Load loop count into register edx

// (with volatile)
// loop: 
//     mov esi, [a]  ; Load value of 'a' from memory into register esi
//     add esi, [b]  ; Add value of 'b' from memory to esi
//     mov ecx, esi  ; Store the sum in 'c' (ecx)
//     dec edx       ; Decrement loop counter
//     jnz loop      ; Jump back to loop if not zero
//     ; ecx now holds the final value of 'c'