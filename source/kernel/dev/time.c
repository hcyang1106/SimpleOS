#include "cpu/cpu.h"
#include "dev/time.h"
#include "comm/cpu_instr.h"
#include "os_cfg.h"
#include "core/task.h"

static uint32_t sys_tick; // bss variables are always set to zero

void exception_handler_timer(void);

static void init_pit(void) {
    uint32_t reload_count = PIT_OSC_FREQ / (1000.0 / OS_TICK_MS);

    outb(PIT_COMMAND_MODE_PORT, PIT_CHANNLE0 | PIT_LOAD_LOHI | PIT_MODE3);
    outb(PIT_CHANNEL0_DATA_PORT, reload_count & 0xFF); // load the lower byte
    outb(PIT_CHANNEL0_DATA_PORT, (reload_count >> 8) & 0xFF); // load the higher byte
}

void do_handler_timer(exception_frame_t *frame) {
    sys_tick++;
    pic_send_eoi(IRQ0_TIMER); 

    // (important) task_time_tick should be after pic_send_eoi,
    // otherwise after switching task pic_send_eoi won't be executed (it goes to somewhere else)
    // when main switches to init_task, it starts from init_task_entry so pic_send_eoi is not executed
    task_time_tick(); 
}

void time_init(void) {
    sys_tick = 0;
    init_pit();
    irq_install(IRQ0_TIMER, exception_handler_timer);
    irq_enable(IRQ0_TIMER);
}