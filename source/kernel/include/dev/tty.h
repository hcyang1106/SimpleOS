#ifndef TTY_H
#define TTY_H

#include "ipc/sem.h"
#include "tools/buffer.h"

#define TTY_NUM 8 

#define TTY_OBUF_SIZE 512
#define TTY_IBUF_SIZE 512

#define TTY_OCRLF (1 << 0) // if this is turned on, then we send '\r''\n' to console layer
// #define TTY_ICRLF (1 << 0) // don't think this is needed
#define TTY_IECHO (1 << 1)

#define TTY_CMD_ECHO 0x1

// typedef struct _tty_fifo_t {
//     char *buf;
//     int size;
//     int read, write;
//     int count;
// }tty_fifo_t;

typedef struct _tty_t {
    char obuf[TTY_OBUF_SIZE];
    char ibuf[TTY_IBUF_SIZE];
    sem_t osem;
    sem_t isem_empty;
    sem_t isem_full;
    fifo_t ofifo;
    fifo_t ififo;
    int oflags;
    int iflags;
    int console_idx;
}tty_t;

int tty_fifo_get(fifo_t *fifo, char *c);
int tty_fifo_put(fifo_t *fifo, char c);
void tty_in(char c);
void tty_select(int idx);

#endif