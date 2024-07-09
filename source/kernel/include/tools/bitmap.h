#ifndef BITMAP_H
#define BITMAP_H

#include "comm/types.h"

typedef struct _bitmap_t {
    int bit_count; // record total pages
    uint8_t *start;
}bitmap_t;

void bitmap_init(bitmap_t *bitmap, uint8_t *start, int bit_count, int value);
int bitmap_get_bit(bitmap_t *bitmap, int index);
void bitmap_set_bit(bitmap_t *bitmap, int index, int count, int value);
int bitmap_is_set(bitmap_t *bitmap, int index);
int bitmap_alloc_nbits(bitmap_t *bitmap, int bit, int count);
int bitmap_byte_count (int bit_count);

#endif