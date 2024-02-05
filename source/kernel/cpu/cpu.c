#include "cpu/cpu.h"
#include "os_cfg.h"
#include "comm/cpu_instr.h"

static segment_desc_t gdt_table[GDT_TABLE_SIZE];
static gate_desc_t idt_table[IDT_TABLE_SIZE];

void exception_handler_unknown(void);
void exception_handler_divider(void);


static void do_default_handler(exception_frame_t *frame, char *msg) {
    for (;;) {}
}

void do_handler_unknown(exception_frame_t *frame) {
    do_default_handler(frame, "unknown exception");
}

void do_handler_divider(exception_frame_t *frame) {
    do_default_handler(frame, "Divider exception");
}

void segment_desc_set(int selector, uint32_t base, uint32_t limit, uint16_t attr) {
    segment_desc_t *desc = gdt_table + selector / sizeof(segment_desc_t);
    
    if (limit > 0xFFFFF) {
        // if limit could not be stored in 20 bits,
        // set G to 1 and divide limit by 4KB
        attr |= 0x8000;
        limit /= 0x1000;
    }

    desc->limit15_0 = limit & 0xFFFF;
    desc->base15_0 = base & 0xFFFF;
    desc->base23_16 = (base >> 16) & 0xFF;
    desc->attr = attr | (((limit >> 16) & 0xF) << 8);
    desc->base31_24 = (base >> 24) & 0xFF;
}

void gate_desc_set(gate_desc_t *desc, uint16_t selector, uint32_t offset, uint16_t attr) {
    desc->offset15_0 = offset &0xFFFF;
    desc->selector = selector;
    desc->attr = attr;
    desc->offset31_16 = offset >> 16; 
}

void init_gdt(void) {
    for (int i = 0; i < GDT_TABLE_SIZE; i++) {
        segment_desc_set(i * sizeof(segment_desc_t), 0, 0, 0);
    }

    segment_desc_set(KERNEL_SELECTOR_CS, 0, 0xFFFFFFFF, SEG_P_PRESENT | SEG_DPL0 |
     SEG_S_NORMAL | SEG_TYPE_CODE | SEG_TYPE_RW | SEG_D | SEG_G);
    segment_desc_set(KERNEL_SELECTOR_DS, 0, 0xFFFFFFFF, SEG_P_PRESENT | SEG_DPL0 |
     SEG_S_NORMAL | SEG_TYPE_DATA | SEG_TYPE_RW | SEG_D | SEG_G) ;

    lgdt((uint32_t)gdt_table, sizeof(gdt_table));
}

void init_idt(void) {
    for (int i = 0; i < IDT_TABLE_SIZE; i++) {
        gate_desc_set(idt_table + i, KERNEL_SELECTOR_CS, (uint32_t)exception_handler_unknown, GATE_P_PRESENT | GATE_DPL0 |
         GATE_D | GATE_TYPE_INT); // offset is function address
    }

    gate_desc_set(idt_table + IRQ0_DE, KERNEL_SELECTOR_CS, (uint32_t)exception_handler_divider, GATE_P_PRESENT | GATE_DPL0 |
         GATE_D | GATE_TYPE_INT);

    lidt((uint32_t)idt_table, sizeof(idt_table));
}

void cpu_init(void) {
    init_gdt();
    init_idt();
}