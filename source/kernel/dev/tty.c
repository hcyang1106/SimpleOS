#include "dev/tty.h"
#include "dev/dev.h"
#include "dev/kbd.h"
#include "dev/console.h"
#include "ipc/sem.h"
#include "tools/log.h"
#include "cpu/cpu.h"

static tty_t tty_devs[TTY_NUM];
static int curr_tty = 0;

static tty_t *get_tty(device_t *dev) {
    int tty = dev->minor;
    if (tty < 0 || tty >= TTY_NUM || (!dev->open_count)) {
        log_printf("tty is not opened. tty=%d", tty);
        return (tty_t*)0;
    }

    return tty_devs + tty;
}

// can view tty_open, dev_open as doing initialization
int tty_open(device_t *dev) {
    int idx = dev->minor;
    if ((idx < 0) || idx >= TTY_NUM) {
        return -1;
    }

    tty_t *tty = tty_devs + idx;
    fifo_init(&tty->ofifo, tty->obuf, TTY_OBUF_SIZE);
    sem_init(&tty->osem, TTY_OBUF_SIZE);
    fifo_init(&tty->ififo, tty->ibuf, TTY_IBUF_SIZE);
    sem_init(&tty->isem_empty, 0); // empty sem
    sem_init(&tty->isem_full, TTY_IBUF_SIZE);

    tty->console_idx = idx;
    tty->oflags = TTY_OCRLF;
    // tty->iflags = TTY_IECHO | TTY_ICRLF;
    tty->iflags = TTY_IECHO;
    console_init(idx);
    kbd_init();

    return 0;
}

// how to write to devices efficiently?
// we write data to buffer, then wait until the first data could be written
// the rest of the data is written in interrupt handler
// after hardware finished writing, it causes interrupt and interrupt handler
// continues to write data
// it still needs the time to "write data", but reduces the time of "waiting hardware to finish"

// consumer/producer problem needs two counting semaphores (example put at the end of this file)
// however here we don't need the "empty" semaphore because interrupt will take care of it
int tty_write(device_t *dev, int addr, char *buf, int size) {
    if (size < 0) {
        return -1;
    }

    tty_t *tty = get_tty(dev);
    if (!tty) {
        return -1;
    }

    // actually the buffer here won't grow
    int len = 0;
    while(size) {
        char c = *buf++;

        if (c == '\n' && (tty->oflags & TTY_OCRLF)) {
            sem_wait(&tty->osem);
            int err = fifo_put(&tty->ofifo, '\r');
            if (err < 0) {
                break;
            }
        }

        sem_wait(&tty->osem);
        int err = fifo_put(&tty->ofifo, c);
        if (err < 0) {
            break;
        }

        len++;
        size--;

        console_write(tty);
    }

    return len;
}

int tty_read(device_t *dev, int addr, char *buf, int size) {
    if (size < 0) {
        return -1;
    }

    tty_t *tty = get_tty(dev);
    if (!tty) {
        return -1;
    }

    int len = 0;
    char *p = buf;
    while (len < size) {
        sem_wait(&tty->isem_empty);
        char ch;
        int err = fifo_get(&tty->ififo, &ch);
        if (err < 0) {
            break;
        }
        sem_notify(&tty->isem_full);

        switch (ch)
        {
        case '\x7f':
            if (len == 0) {
                continue;
            }
            p--;
            len--;
            break;
        // case '\n':
        //     if ((tty->iflags & TTY_ICRLF)) {
        //         *p++ = '\r';
        //         len++;
        //     }
        //     if (len < size) {
        //         *p++ = '\n';
        //         len++;
        //     }
        //     break;
        default:
            *p++ = ch;
            len++;
            break;
        }

        if (tty->iflags & TTY_IECHO) {
            tty_write(dev, 0, &ch, 1); // how console reacts
        }

        if (ch == '\n' || ch == '\r') {
            break;
        }
    }

    return len; // newlib requires to be len
}

int tty_control(device_t *dev, int cmd, int arg0, int arg1) {
    tty_t *tty = get_tty(dev);
    if (!tty) {
        return -1;
    }

    switch (cmd) {
        case TTY_CMD_ECHO:
            if (arg0) {
                tty->iflags |= TTY_IECHO;
            } else {
                tty->iflags &= ~TTY_IECHO;
            }
            break;
        default:
            break;
    }

    return 0;
}

int tty_close(device_t *dev) {

}

// called by interrupt handler
// if writes a character, it uses sem to notify process
void tty_in(char c) {
    sem_wait(&tty_devs[curr_tty].isem_full);
    fifo_put(&tty_devs[curr_tty].ififo, c);
    sem_notify(&tty_devs[curr_tty].isem_empty);
}

void tty_select(int idx) {
    if (idx < 0 || idx >= TTY_NUM) {
        log_printf("invalid tty index");
        return;
    }
    console_select(idx);
    curr_tty = idx;
}

dev_desc_t dev_tty_desc = {
    .name = "tty",
    .major = DEV_TTY,
    .open = tty_open,
    .read = tty_read,
    .write = tty_write,
    .control = tty_control,
    .close = tty_close,
};

// example of producer/consumer

// full is initialized as N (buffer size)
// empty is initialized as 0
// mutex is used to protect race condition

// Producer:

// do{

// //produce an item

// wait(full);
// wait(mutex);

// //place in buffer

// signal(mutex);
// signal(empty);

// }while(true)

// Consumer:

// do{

// wait(empty);
// wait(mutex);

// // consume item from buffer

// signal(mutex);
// signal(full);

// }while(true)