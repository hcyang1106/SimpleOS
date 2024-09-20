#ifndef CONSOLE_H
#define CONSOLE_H

#include "comm/types.h"
#include "dev/tty.h"
#include "ipc/mutex.h"

#define CONSOLE_DISP_START 0xb8000
#define CONSOLE_DISP_END (0xb8000 + 32 * 1024)
#define CONSOLE_ROW 25
#define CONSOLE_COL 80

#define ASCII_ESC 0x1b

#define ESC_PARAM_MAX 10

typedef enum {
    COLOR_BLACK = 0,
    COLOR_BLUE,
    COLOR_GREEN,
    COLOR_CYAN,
    COLOR_RED,
    COLOR_MAGENTA,
    COLOR_BROWN,
    COLOR_GRAY,
    COLOR_DARK_GRAY,
    COLOR_LIGHT_BLUE,
    COLOR_LIGHT_GREEN,
    COLOR_LIGHT_CYAN,
    COLOR_LIGHT_RED,
    COLOR_LIGHT_MAGENTA,
    COLOR_YELLOW,
    COLOR_WHITE,
} color_t;

typedef union _disp_char_t {
    struct {
        char ch;
        char foreground: 4;
        char background: 3;
    };
    uint16_t v;
}disp_char_t;

typedef struct _console_t {
    enum {
        CONSOLE_WRITE_NORMAL,
        CONSOLE_WRITE_ESC,
        CONSOLE_WRITE_SEQ,
    }write_state;

    disp_char_t *disp_base;
    int cursor_row, cursor_col;
    int disp_rows, disp_cols;
    color_t foreground, background;
    int prev_cursor_row, prev_cursor_col;
    int esc_param[ESC_PARAM_MAX];
    int curr_param_idx;
    mutex_t mutex;
}console_t;

int console_init(int idx);
int console_write(tty_t *tty);
void console_close(int console);
void console_select(int idx);

#endif