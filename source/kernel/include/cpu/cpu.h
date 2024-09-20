#ifndef CPU_H
#define CPU_H

#include "comm/types.h"

#define EFLAGS_IF           (1 << 9)
#define EFLAGS_DEFAULT      (1 << 1)

#pragma pack(1)

typedef struct _tss_t {
    uint32_t prelink;
    uint32_t esp0, ss0, esp1, ss1, esp2, ss2;
    uint32_t cr3;
    uint32_t eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint32_t iomap;
}tss_t;

// _segment_desc_t can be ignored
typedef struct _segment_desc_t { 
    uint16_t limit15_0;
    uint16_t base15_0;
    uint8_t base23_16;
    uint16_t attr;
    uint8_t base31_24;
}segment_desc_t;

typedef struct _gate_desc_t {
    uint16_t offset15_0;
    uint16_t selector;
    uint16_t attr;
    uint16_t offset31_16;
}gate_desc_t;

#pragma pack()

typedef struct _exception_frame_t {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t num, error_code;
    uint32_t eip, cs, eflags;
    uint32_t esp3, ss3;
}exception_frame_t;

typedef void(*irq_handler_t)(void); // defines a type irq_handler_t which is a pointer to void(*)(void)

void cpu_init(void);
void pic_send_eoi(int irq_num);

void irq_install(int irq_num, irq_handler_t handler);
void segment_desc_set(int selector, uint32_t base, uint32_t limit, uint16_t attr);

void irq_enable_global(void);
void irq_disable_global(void);
void irq_enable(int irq_num);
void irq_disable(int irq_num);

typedef uint32_t irq_state_t;
irq_state_t irq_enter_protection(void);
void irq_leave_protection(irq_state_t state);

int gdt_alloc_desc(void); // find an unused space in gdt
void gdt_free_sel(int sel);

void switch_to_tss(int tss_sel);

// for gdt descriptors
#define SEG_G (1 << 15) // granularity is 4KB
#define SEG_D (1 << 14) // set to 32 bits mode
#define SEG_P_PRESENT (1 << 7) // descriptor exists

#define SEG_DPL0 (0 << 5)
#define SEG_DPL3 (3 << 5)

#define SEG_CPL0 (0 << 0)
#define SEG_CPL3 (3 << 0)

#define SEG_S_SYSTEM (0 << 4)
#define SEG_S_NORMAL (1 << 4)

#define SEG_TYPE_CODE (1 << 3)
#define SEG_TYPE_DATA (0 << 3)
#define SEG_TSS (9 << 0)

#define SEG_TYPE_RW (1 << 1) // both read and write available

// for idt descriptors
#define GATE_P_PRESENT (1 << 15)
#define GATE_DPL0 (0 << 13)
#define GATE_DPL3 (3 << 13)
#define GATE_D (1 << 11)
#define GATE_TYPE_INT (0xE << 8)
#define GATE_TYPE_SYSCALL (0xC << 8)

#define IRQ0_DE 0
#define IRQ1_DB 1
#define IRQ2_NMI 2
#define IRQ3_BP 3
#define IRQ4_OF 4
#define IRQ5_BR 5
#define IRQ6_UD 6
#define IRQ7_NM 7
#define IRQ8_DF 8
#define IRQ10_TS 10
#define IRQ11_NP 11
#define IRQ12_SS 12
#define IRQ13_GP 13
#define IRQ14_PF 14
#define IRQ16_MF 16
#define IRQ17_AC 17
#define IRQ18_MC 18
#define IRQ19_XM 19
#define IRQ20_VE 20

#define IRQ0_TIMER          0x20
#define IRQ1_KEYBOARD       0x21
#define IRQ14_DISK_PRIMARY  0x2E 

#define PIC0_ICW1			0x20
#define PIC0_ICW2			0x21
#define PIC0_ICW3			0x21
#define PIC0_ICW4			0x21
#define PIC0_OCW2			0x20
#define PIC0_IMR			0x21

#define PIC1_ICW1			0xa0
#define PIC1_ICW2			0xa1
#define PIC1_ICW3			0xa1
#define PIC1_ICW4			0xa1
#define PIC1_OCW2			0xa0
#define PIC1_IMR			0xa1

#define PIC_ICW1_ICW4		(1 << 0)
#define PIC_ICW1_ALWAYS_1	(1 << 4)
#define PIC_ICW4_8086	    (1 << 0)
#define PIC_OCW2_EOI		(1 << 5)
#define IRQ_PIC_START		0x20

#define ERR_PAGE_P (1 << 0)
#define ERR_PAGE_WR (1 << 1)
#define ERR_PAGE_US (1 << 2)

#define ERR_EXT (1 << 0)
#define ERR_IDT (1 << 1)

#endif
