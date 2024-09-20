#ifndef BUFFER_H
#define BUFFER_H

typedef struct _fifo_t {
    char *buf;
    int size;
    int read, write;
    int count;
}fifo_t;

// static inline int fifo_get_count(fifo_t *fifo) {
//     return fifo->count;
// }
int fifo_put(fifo_t *fifo, char c);
int fifo_put_sector_size(fifo_t *fifo, char *c);
int fifo_get(fifo_t *fifo, char *c);
int fifo_get_sector_size(fifo_t *fifo, char **c);
void fifo_init(fifo_t *fifo, char *buf, int size);
void fifo_reset(fifo_t *fifo);

#endif