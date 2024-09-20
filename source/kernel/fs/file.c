#include "fs/file.h"
#include "ipc/mutex.h"
#include "tools/klib.h"
#include "fs/file.h"

// bss has been zeroed (initial values are zeroes)
static file_t file_table[FILE_TABLE_SIZE];

static mutex_t file_table_mutex;

void file_table_init(void) {
    mutex_init(&file_table_mutex);
    kernel_memset(file_table, 0, sizeof(file_table));
}

// free the occupied space
void file_free(file_t *file) {
    mutex_lock(&file_table_mutex);

    if (file->ref) {
        file->ref--;
    }

    mutex_unlock(&file_table_mutex);
}

// find an empty space in file_table
// using ref
file_t *file_alloc(void) {
    file_t *p = (file_t*)0;
    mutex_lock(&file_table_mutex);

    for (int i = 0; i < FILE_TABLE_SIZE; i++) {
        p = file_table + i;
        if (p->ref == 0) {
            kernel_memset(p, 0, sizeof(file_t));
            p->ref = 1;
            break;
        }
    }

    mutex_unlock(&file_table_mutex);
    return p;
}

void file_inc_ref(file_t *file) {
    mutex_lock(&file_table_mutex);
    file->ref++;
    mutex_unlock(&file_table_mutex);
}