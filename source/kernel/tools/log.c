#include "tools/log.h"
#include "comm/cpu_instr.h" // search directory is /source and /source/include, so comm must be added
#include <stdarg.h>
#include "tools/klib.h"
#include "cpu/cpu.h"
#include "ipc/mutex.h"
#include "dev/console.h"
#include "dev/dev.h"

#define LOG_USE_COM 0
#define COM1_PORT 0x3F8

static mutex_t mutex;
static int log_dev_id;

// initialization for RS-232
void log_init(void) {
    mutex_init(&mutex);
    log_dev_id = dev_open(DEV_TTY, 0, (void*)0);

#if LOG_USE_COM
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);   
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0F);
#endif

}

// qemu is able to display the data going through RS-232,
// and we are using it to display the log
// log_printf "automatically" adds \n at the end
void log_printf(const char *fmt, ...) {
    char str_buf[128];
    kernel_memset(str_buf, '\0', sizeof(str_buf)); // important!

    va_list args;
    va_start(args, fmt); // the second argument is to pass "the last parameter" that is not "variable arguments"
    kernel_vsprintf(str_buf, fmt, args);
    va_end(args);

    // irq_state_t state = irq_enter_protection();
    mutex_lock(&mutex);

#if LOG_USE_COM
    char *p = str_buf;
    while (*p) {
        // check if it is busy or not (check if the sixth bit is zero)
        while (inb(COM1_PORT + 5) & (1 << 6) == 0);
        outb(COM1_PORT, *p);
        p++;
    }
    
    outb(COM1_PORT, '\r');
    outb(COM1_PORT, '\n');
#endif

    // console_write(0, str_buf, kernel_strlen(str_buf));
    // we don't need '\r' because out code make '\n' do both things
    // console_write(0, "\n", 1);
    dev_write(log_dev_id, 0, "log:", 4);
    dev_write(log_dev_id, 0, str_buf, kernel_strlen(str_buf));
    char c = '\n';
    dev_write(log_dev_id, 0, &c, 1);

    // irq_leave_protection(state);
    mutex_unlock(&mutex);
}


// const char *a => can change the object pointing to, but cannot modify the value
// char* const a => cannot change the object pointing to, but can modify the value