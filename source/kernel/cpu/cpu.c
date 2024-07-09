#include "cpu/cpu.h"
#include "os_cfg.h"
#include "comm/cpu_instr.h"
#include "tools/log.h"
#include "ipc/mutex.h"

static mutex_t mutex;

void exception_handler_unknown (void);
void exception_handler_divider (void);
void exception_handler_Debug (void);
void exception_handler_NMI (void);
void exception_handler_breakpoint (void);
void exception_handler_overflow (void);
void exception_handler_bound_range (void);
void exception_handler_invalid_opcode (void);
void exception_handler_device_unavailable (void);
void exception_handler_double_fault (void);
void exception_handler_invalid_tss (void);
void exception_handler_segment_not_present (void);
void exception_handler_stack_segment_fault (void);
void exception_handler_general_protection (void);
void exception_handler_page_fault (void);
void exception_handler_fpu_error (void);
void exception_handler_alignment_check (void);
void exception_handler_machine_check (void);
void exception_handler_smd_exception (void);
void exception_handler_virtual_exception (void);

static segment_desc_t gdt_table[GDT_TABLE_SIZE];
static gate_desc_t idt_table[IDT_TABLE_SIZE];

// print regs when exception/interrupt happens
static void dump_core_regs (exception_frame_t * frame) {
    log_printf("IRQ: %d, error code: %d.", frame->num, frame->error_code);
    log_printf("CS: %d\nDS: %d\nES: %d\nSS: %d\nFS: %d\nGS: %d",
               frame->cs, frame->ds, frame->es, frame->ds, frame->fs, frame->gs
    );
    log_printf("EAX: 0x%x\n"
                "EBX: 0x%x\n"
                "ECX: 0x%x\n"
                "EDX: 0x%x\n"
                "EDI: 0x%x\n"
                "ESI: 0x%x\n"
                "EBP: 0x%x\n"
                "ESP: 0x%x\n",
               frame->eax, frame->ebx, frame->ecx, frame->edx,
               frame->edi, frame->esi, frame->ebp, frame->esp);
    log_printf("EIP: 0x%x\nEFLAGS: 0x%x\n", frame->eip, frame->eflags);
}

static void do_default_handler(exception_frame_t *frame, char *msg) {
    log_printf("--------------------");
    log_printf("IRQ/Exception happend: %s", msg);
    dump_core_regs(frame);

    for (;;) {
        hlt();
    }
}

void do_handler_unknown(exception_frame_t *frame) {
    do_default_handler(frame, "Unknown Exception");
}

void do_handler_divider(exception_frame_t *frame) {
    do_default_handler(frame, "Divider Exception");
}

void do_handler_Debug(exception_frame_t * frame) {
	do_default_handler(frame, "Debug Exception");
}

void do_handler_NMI(exception_frame_t * frame) {
	do_default_handler(frame, "NMI Interrupt");
}

void do_handler_breakpoint(exception_frame_t * frame) {
	do_default_handler(frame, "Breakpoint");
}

void do_handler_overflow(exception_frame_t * frame) {
	do_default_handler(frame, "Overflow Exception");
}

void do_handler_bound_range(exception_frame_t * frame) {
	do_default_handler(frame, "BOUND Range Exceeded Exception");
}

void do_handler_invalid_opcode(exception_frame_t * frame) {
	do_default_handler(frame, "Invalid Opcode Exception");
}

void do_handler_device_unavailable(exception_frame_t * frame) {
	do_default_handler(frame, "Device Not Available");
}

void do_handler_double_fault(exception_frame_t * frame) {
	do_default_handler(frame, "Double Fault");
}

void do_handler_invalid_tss(exception_frame_t * frame) {
	do_default_handler(frame, "Invalid TSS Exception");
}

void do_handler_segment_not_present(exception_frame_t * frame) {
	do_default_handler(frame, "Segment Not Present Exception");
}

void do_handler_stack_segment_fault(exception_frame_t * frame) {
	do_default_handler(frame, "Stack Segment Fault");
}

void do_handler_general_protection(exception_frame_t * frame) {
	do_default_handler(frame, "General Protection");
}

void do_handler_page_fault(exception_frame_t * frame) {
	do_default_handler(frame, "Page Fault Exception");
}

void do_handler_fpu_error(exception_frame_t * frame) {
	do_default_handler(frame, "X87 FPU Floating Point Error");
}

void do_handler_alignment_check(exception_frame_t * frame) {
	do_default_handler(frame, "Alignment Check");
}

void do_handler_machine_check(exception_frame_t * frame) {
	do_default_handler(frame, "Machine Check");
}

void do_handler_smd_exception(exception_frame_t * frame) {
	do_default_handler(frame, "SIMD Floating Point Exception");
}

void do_handler_virtual_exception(exception_frame_t * frame) {
	do_default_handler(frame, "Virtualization Exception");
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

// exported
static void gate_desc_set(gate_desc_t *desc, uint16_t selector, uint32_t offset, uint16_t attr) {
    desc->offset15_0 = offset &0xFFFF;
    desc->selector = selector;
    desc->attr = attr;
    desc->offset31_16 = offset >> 16; 
}

static void init_gdt(void) {
    for (int i = 0; i < GDT_TABLE_SIZE; i++) {
        segment_desc_set(i * sizeof(segment_desc_t), 0, 0, 0);
    }

    segment_desc_set(KERNEL_SELECTOR_CS, 0, 0xFFFFFFFF, SEG_P_PRESENT | SEG_DPL0 |
     SEG_S_NORMAL | SEG_TYPE_CODE | SEG_TYPE_RW | SEG_D | SEG_G);
    segment_desc_set(KERNEL_SELECTOR_DS, 0, 0xFFFFFFFF, SEG_P_PRESENT | SEG_DPL0 |
     SEG_S_NORMAL | SEG_TYPE_DATA | SEG_TYPE_RW | SEG_D | SEG_G) ;

    lgdt((uint32_t)gdt_table, sizeof(gdt_table));
}

// initialize 8259 chip (a prgrammable interrupt controller)
static void init_pic(void) {
    outb(PIC0_ICW1, PIC_ICW1_ALWAYS_1 | PIC_ICW1_ICW4);
    // interrupt number starts from 0x20 (number smaller than 0x20 are already occupied by preset interrupts)
    outb(PIC0_ICW2, IRQ_PIC_START);
    // this means IRQ2 is connected with another 8259 chip
    outb(PIC0_ICW3, 1 << 2);
    // 8086 mode
    outb(PIC0_ICW4, PIC_ICW4_8086);

    outb(PIC1_ICW1, PIC_ICW1_ICW4 | PIC_ICW1_ALWAYS_1);
    // the interrupt number starts from 0x20 + 8
    outb(PIC1_ICW2, IRQ_PIC_START + 8);
    // connected to PIC0
    outb(PIC1_ICW3, 2);
    outb(PIC1_ICW4, PIC_ICW4_8086);

    // disable all interrupts except the signal sent to IRQ2
    // since we're masking PIC1, there's no need to disable IRQ2 of PCI0;
    outb(PIC0_IMR, 0xFF & ~(1 << 2));
    outb(PIC1_IMR, 0xFF);
}

// exception handler must end with "iret", therefore we must write the function in assembly.
// iret is used to pop back regs
static void init_idt(void) {
    for (int i = 0; i < IDT_TABLE_SIZE; i++) {
        irq_install(i, exception_handler_unknown);
        // gate_desc_set(idt_table + i, KERNEL_SELECTOR_CS, (uint32_t)exception_handler_unknown, GATE_P_PRESENT | GATE_DPL0 |
        //  GATE_D | GATE_TYPE_INT); // offset is function address
    }

    irq_install(IRQ0_DE, exception_handler_divider);
    irq_install(IRQ1_DB, exception_handler_Debug);
    irq_install(IRQ2_NMI, exception_handler_NMI);
    irq_install(IRQ3_BP, exception_handler_breakpoint);
    irq_install(IRQ4_OF, exception_handler_overflow);
    irq_install(IRQ5_BR, exception_handler_bound_range);
    irq_install(IRQ6_UD, exception_handler_invalid_opcode);
    irq_install(IRQ7_NM, exception_handler_device_unavailable);
    irq_install(IRQ8_DF, exception_handler_double_fault);
    irq_install(IRQ10_TS, exception_handler_invalid_tss);
    irq_install(IRQ11_NP, exception_handler_segment_not_present);
    irq_install(IRQ12_SS, exception_handler_stack_segment_fault);
    irq_install(IRQ13_GP, exception_handler_general_protection);
    irq_install(IRQ14_PF, exception_handler_page_fault);
    irq_install(IRQ16_MF, exception_handler_fpu_error);
    irq_install(IRQ17_AC, exception_handler_alignment_check);
    irq_install(IRQ18_MC, exception_handler_machine_check);
    irq_install(IRQ19_XM, exception_handler_smd_exception);
    irq_install(IRQ20_VE, exception_handler_virtual_exception);

    lidt((uint32_t)idt_table, sizeof(idt_table));

    init_pic();
}

// exported
void irq_install(int irq_num, irq_handler_t handler) {
    gate_desc_set(idt_table + irq_num, KERNEL_SELECTOR_CS, (uint32_t)handler, GATE_P_PRESENT | GATE_DPL0 |
         GATE_D | GATE_TYPE_INT);
}

void pic_send_eoi(int irq_num) {
    irq_num -= IRQ_PIC_START;

    if (irq_num >= 8) {
        outb(PIC1_OCW2, PIC_OCW2_EOI);
    }

    outb(PIC0_OCW2, PIC_OCW2_EOI);
}

void irq_enable(int irq_num) {
    if (irq_num < IRQ_PIC_START) {
        return;
    }

    int irq_bit = irq_num - IRQ_PIC_START;
    if (irq_bit < 8) {
        uint8_t mask = inb(PIC0_IMR) & ~(1 << irq_bit);
        outb(PIC0_IMR, mask);
        return;
    }
    irq_bit -= 8;
    uint8_t mask = inb(PIC1_IMR) & ~(1 << irq_bit);
    outb(PIC1_IMR, mask);
    return;
}

void irq_disable(int irq_num) {
    if (irq_num < IRQ_PIC_START) {
        return;
    }

    int irq_bit = irq_num - IRQ_PIC_START;
    if (irq_bit < 8) {
        uint8_t mask = inb(PIC0_IMR) | (1 << irq_bit);
        outb(PIC0_IMR, mask);
        return;
    }
    irq_bit -= 8;
    uint8_t mask = inb(PIC1_IMR) | (1 << irq_bit);
    outb(PIC1_IMR, mask);
    return;
}

void irq_enable_global(void) {
    sti(); // it sets EFLAGS IF flag to 1
}

void irq_disable_global(void) {
    cli(); // it sets EFLAGS IF flag to 0
}

void cpu_init(void) {
    // mutex_init(&mutex);
    init_gdt();
    init_idt();
}

int gdt_alloc_desc(void) {
    // irq_state_t state = irq_enter_protection();
    mutex_lock(&mutex);

    // i should begin with 1 because 0 is not used
    for (int i = 1; i < GDT_TABLE_SIZE; i++) {
        if (gdt_table[i].attr == 0) {
            // irq_leave_protection(state);
            // mutex_unlock(&mutex);
            return i * sizeof(segment_desc_t);
        }
    }

    // irq_leave_protection(state);
    mutex_unlock(&mutex);

    return -1;
}

void gdt_free_sel(int sel) {
    mutex_lock(&mutex);

    gdt_table[sel / sizeof(segment_desc_t)].attr = 0;

    mutex_unlock(&mutex);
}

void switch_to_tss(int tss_sel) {
    far_jump(tss_sel, 0);
}

// Q: why can't we just simply use irq_disable_global and irq_enable_global
// to control critical section?
// A: if interrupt is "disabled" at first, then we call irq_disable_global -> irq_enable_global
// will make the interrupt "enabled", which is inconsistent

irq_state_t irq_enter_protection(void) {
    irq_state_t state = read_eflags();
    irq_disable_global();
    return state;
}

void irq_leave_protection(irq_state_t state) {
    // instead of enabling the interrupt, we revocer eflags
    // irq_enable_global();
    write_eflags(state);
}