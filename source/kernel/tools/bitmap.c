#include "tools/bitmap.h"
#include "tools/klib.h"
#include "comm/types.h"

int bitmap_byte_count (int bit_count) {
    return (bit_count + 8 - 1) / 8;
}

// interpretation of hex numbers:
// https://stackoverflow.com/questions/4737798/unsigned-hexadecimal-constant-in-c
void bitmap_init(bitmap_t *bitmap, uint8_t *start, int bit_count, int value) {
    bitmap->bit_count = bit_count;
    bitmap->start = start;
    kernel_memset(start, value ? 0xFF : 0, (bit_count + 8 - 1) / 8); // 
}

int bitmap_get_bit(bitmap_t *bitmap, int index) {
    int byte_idx = index / 8;
    uint8_t byte = *(bitmap->start + byte_idx);
    return byte & (1 << (index % 8));
}

void bitmap_set_bit(bitmap_t *bitmap, int index, int count, int value) {
    for (int i = 0; i < count && (index + count - 1 < bitmap->bit_count); i++) {
        // should already be 1
        int pre_bit = bitmap->start[(index + i) / 8] & (1 << ((index + i) % 8));
        ASSERT(value ? !pre_bit : pre_bit);

        if (value) {
            bitmap->start[(index + i) / 8] |= (1 << ((index + i) % 8));
        } else {
            // originally i wrote the line below which is wrong
            // it also eliminates its neighbor pages 
            // bitmap->start[(index + i) / 8] &= (0 << ((index + i) % 8));      
            bitmap->start[(index + i) / 8] &= ~(1 << ((index + i) % 8));      
        }  
    }
}

int bitmap_is_set(bitmap_t *bitmap, int index) {
    return bitmap->start[index / 8] & (1 << (index % 8));
}

int bitmap_alloc_nbits(bitmap_t *bitmap, int bit, int count) {
    int start_idx = 0;
    while(1) {
        while (bitmap_get_bit(bitmap, start_idx) != bit && start_idx < bitmap->bit_count) {
            start_idx++;
        }

        if (start_idx >= bitmap->bit_count) {
            return -1;
        }

        int check_idx;
        for (check_idx = start_idx; check_idx < start_idx+count && check_idx < bitmap->bit_count; check_idx++) {
            if (bitmap_get_bit(bitmap, check_idx) != bit) {
                break;
            }
        }

        if (check_idx == start_idx + count) {
            bitmap_set_bit(bitmap, start_idx, count, 1);
            return start_idx;
        }
        
        start_idx = check_idx + 1;
    }

    return -1;
}



