#include "tools/buffer.h"
#include "cpu/cpu.h"
#include "os_cfg.h"
#include "tools/klib.h"
#include "tools/log.h"

// read and write can be at the same time
// therefore need protection
int fifo_put(fifo_t *fifo, char c) {
    irq_state_t state = irq_enter_protection();
    if (fifo->count >= fifo->size) {
        irq_leave_protection(state);
        return -1;
    }

    fifo->buf[fifo->write++] = c;
    if (fifo->write >= fifo->size) {
        fifo->write = 0;
    }

    fifo->count++;
    irq_leave_protection(state);
    return 0;
}

int fifo_put_sector_size(fifo_t *fifo, char *c) {
    irq_state_t state = irq_enter_protection();

    if (fifo->count + SECTOR_SIZE >= fifo->size) {
        log_printf("fifo space not enough for a sector size");
        irq_leave_protection(state);
        return -1;
    }

    kernel_memcpy(fifo->buf + fifo->write, c, SECTOR_SIZE);
    fifo->write = (fifo->write + SECTOR_SIZE) % fifo->size;
    fifo->count += SECTOR_SIZE;

    irq_leave_protection(state);
    return 0;
}

int fifo_get_sector_size(fifo_t *fifo, char **c) {
    irq_state_t state = irq_enter_protection();

    if (fifo->count == 0) {
        irq_leave_protection(state);
        return -1;
    }
    
    if (fifo->count - SECTOR_SIZE < 0) {
        log_printf("fifo count not enough to get a sector size");
        irq_leave_protection(state);
        return -1;
    }

    *c = fifo->buf + fifo->read;
    fifo->read = (fifo->read + SECTOR_SIZE) % fifo->size;
    fifo->count -= SECTOR_SIZE;

    irq_leave_protection(state);
    return 0;
}

int fifo_get(fifo_t *fifo, char *c) {
    irq_state_t state = irq_enter_protection();
    if (fifo->count <= 0) {
        irq_leave_protection(state);
        return -1;
    }

    *c = fifo->buf[fifo->read++];
    if (fifo->read >= fifo->size) {
        fifo->read = 0;
    }

    fifo->count--;
    irq_leave_protection(state);
    return 0;
}

void fifo_init(fifo_t *fifo, char *buf, int size) {
    fifo->buf = buf;
    fifo->count = 0;
    fifo->read = fifo->write = 0;
    fifo->size = size;
}

void fifo_reset(fifo_t *fifo) {
    fifo->count = 0;
    fifo->read = fifo->write = 0;
}