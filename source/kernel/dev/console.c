// ASCII codes can be divided into two parts,
// one is chars that can be printed directly
// one is chars that is used to "control printing" (e.g. '\n', '\b'...)
// our console has to handle the second part

// we have completed some useful functions in kernel,
// (e.g. clear_display, move_to_col0...)
// however, we also need to let users be able to control these
// so there exist "control printing chars and sequences"

#include "dev/console.h"
#include "tools/klib.h"
#include "comm/cpu_instr.h"
#include "ipc/sem.h"
#include "cpu/cpu.h"
#include "tools/log.h"

#define CONSOLE_NUM 8
static console_t console_buf[CONSOLE_NUM];
static int curr_console_idx = 0;

// cursor position is from 0 to 1999 (25 * 80 = 2000) 
static int read_cursor_pos(void) {
    int pos;

    irq_state_t state = irq_enter_protection();
    outb(0x3D4, 0xF); // the lower 8 bits
    pos = inb(0x3D5);
    outb(0x3D4, 0xE); // the higher 8 bits
    pos |= inb(0x3D5) << 8;
    irq_leave_protection(state);
    return pos;
}

static void update_cursor_pos(console_t *console) {
    uint16_t pos = (console - console_buf) * (console->disp_rows * console->disp_cols);
    pos += console->cursor_col + console->cursor_row * console->disp_cols;

    irq_state_t state = irq_enter_protection();
    outb(0x3D4, 0xF); // the lower 8 bits
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0xE); // the higher 8 bits
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    irq_leave_protection(state);
}

// end is included
static void erase_rows(console_t *console, int start, int end) {
    disp_char_t *ch_start = console->disp_base + console->disp_cols * start;
    disp_char_t *ch_end = console->disp_base + console->disp_cols * (end + 1);
    disp_char_t *p = ch_start;
    for (; p < ch_end; p++) {
        p->ch = ' ';
        p->background = console->background;
        p->foreground = console->foreground;
    }
}

static void scroll_up(console_t *console, int lines) {
    disp_char_t *dest = console->disp_base;
    disp_char_t *src = console->disp_base + lines * console->disp_cols;
    uint32_t size = (console->disp_rows - lines) * console->disp_cols * sizeof(disp_char_t);
    kernel_memcpy(dest, src, size);

    erase_rows(console, console->disp_rows - lines, console->disp_rows - 1);
    console->cursor_row -= lines;
}

static void move_forward(console_t *c, int steps) {
    c->cursor_col += steps;
    while (c->cursor_col >= c->disp_cols) {
        if (c->cursor_row >= c->cursor_row - 1) {
            scroll_up(c, 1);
        } 
        c->cursor_col -= c->disp_cols;
        c->cursor_row++;
    }
    
    return;
}

// if current row isn't enough,
// then go to the last position (not last char) of upper row
static int back_chars(console_t *console, int n) {
    if (!n) {
        return 0;
    }

    while (n) {
        if (console->cursor_col > n-1) {
            console->cursor_col -= n;
            return 0;
        } else if (console->cursor_row > 0) {
            n -= console->cursor_col + 1;
            console->cursor_col = console->disp_cols - 1;
            console->cursor_row--;
        } else {
            console->cursor_row = console->cursor_col = 0;
            break;
        }
        if (n == 0) {
            return 0;
        }   
    }
    
    return -1;
}

// disp_char_t consists of v (which is ch, foreground , background)
// this function does not change the cursor on the screen
// we need to write to specific ports to change it
static void show_char(console_t *console, char ch) {
    int offset = console->cursor_col + console->cursor_row * console->disp_cols;
    disp_char_t *p = console->disp_base + offset;
    p->ch = ch;
    p->foreground = console->foreground;
    p->background = console->background;
    move_forward(console, 1);
}

static void erase_one_char(console_t *console) {
    if (back_chars(console, 1) < 0) {
        return;
    }
    show_char(console, ' ');
    back_chars(console, 1);
}

static void clear_display(console_t *console) {
    int size = console->disp_rows * console->disp_cols;
    disp_char_t *p = console->disp_base;
    for (int i = 0; i < size; i++, p++) {
        p->ch = ' ';
        p->foreground = console->foreground;
        p->background = console->background;
    }

    return;
}

static void move_to_col0(console_t *console) {
    console->cursor_col = 0;
}

// will deal with the case of last row
// move every line up one and clear the last line
static void move_to_next_row(console_t *console) {
    console->cursor_row += 1;
    if (console->cursor_row >= console->disp_rows) {
        scroll_up(console, 1);
    }

    return;
}

int console_init(int idx) {
    console_t *console = console_buf + idx;
    console->disp_rows = CONSOLE_ROW;
    console->disp_cols = CONSOLE_COL;
    
    console->disp_base = (disp_char_t*)CONSOLE_DISP_START + idx * (CONSOLE_ROW * CONSOLE_COL);
    
    console->foreground = COLOR_WHITE;
    console->background = COLOR_BLACK;

    if (idx == 0) {
        int cursor_pos = read_cursor_pos();
        console->cursor_row = cursor_pos / console->disp_cols;
        console->cursor_col = cursor_pos % console->disp_cols;
    } else {
        console->cursor_row = 0;
        console->cursor_col = 0;
        clear_display(console);
    }

    console->prev_cursor_row = 0;
    console->prev_cursor_col = 0;

    console->write_state = CONSOLE_WRITE_NORMAL;

    mutex_init(&console->mutex);

    kernel_memset(console->esc_param, 0, sizeof(console->esc_param));

    // clear_display(&console[i]);

    return 0;
}

static void write_normal_state(console_t *console, char ch) {
    switch (ch) {
        case ASCII_ESC:
            console->write_state = CONSOLE_WRITE_ESC;
            break;
        case '\x7f':
            erase_one_char(console);
            break;
        case '\b':
            back_chars(console, 1);
            break;
        case '\r':
            move_to_col0(console);
            break;
        case '\n':
            // move_to_col0(console);
            move_to_next_row(console);
            break;
        default:
            if ((ch >= ' ') && (ch <= '~')) {
                show_char(console, ch);
            }
            break;
    }
}

static void save_cursor(console_t *console) {
    console->prev_cursor_col = console->cursor_col;
    console->prev_cursor_row = console->cursor_row;
}

static void restore_cursor(console_t *console) {
    console->cursor_col = console->prev_cursor_col;
    console->cursor_row = console->prev_cursor_row;
}

static void clear_esc_param(console_t *console) {
    // for (int i = 0; i <= console->curr_param_idx; i++) {
    //     console->esc_param[i] = 0;
    // }
    kernel_memset(console->esc_param, 0, sizeof(int) * (console->curr_param_idx + 1));
    console->curr_param_idx = 0;
}

static void write_esc_state(console_t *console, char ch) {
    switch (ch) {
        case '[':
            clear_esc_param(console);
            console->write_state = CONSOLE_WRITE_SEQ;
            break;
        case '7':
            save_cursor(console);
            console->write_state = CONSOLE_WRITE_NORMAL;
            break;
        case '8':
            restore_cursor(console);
            console->write_state = CONSOLE_WRITE_NORMAL;
            break;
        default:
            console->write_state = CONSOLE_WRITE_NORMAL;
            break;
    }
}

static void set_font_style(console_t *console) {
    static const color_t color_table[] = {
        COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
        COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
    };

    for (int i = 0; i <= console->curr_param_idx; i++) {
        int param = console->esc_param[i];
        if ((param >= 30) && (param <= 37)) {
            console->foreground = color_table[param - 30];
        } else if ((param >= 40) && (param <= 47)) {
            console->background = color_table[param - 40];
        } else if (param == 39) {
            console->foreground = COLOR_WHITE;
        } else if (param == 49) {
            console->background = COLOR_BLACK;
        }
    }
}

// why not just use clear_display?
// because we have to process the params
// params of this function can be used to erase whole or erase part
static void erase_in_display(console_t *console) {
    ASSERT(console->curr_param_idx >= 0);
    int param = console->esc_param[0];
    if (param == 2) { // whole
        clear_display(console);
        console->cursor_row = console->cursor_col = 0;
    }
}

// also added another layer for params processing
static void move_cursor(console_t *console) {
    ASSERT(console->curr_param_idx >= 1);
    console->cursor_row = console->esc_param[0];
    console->cursor_col = console->esc_param[1];
}

// cannot use back_chars because it cleans up the char
// but we're just moving the cursor
// this WON'T move cursor to the upper line (at most the leftmost col)
static void move_left(console_t *console) {
    ASSERT(console->curr_param_idx >= 0);
    // back_chars(console, console->esc_param[0]);
    int n = !console->esc_param[0] ? 1 : console->esc_param[0]; // if zero, then set to 1
    console->cursor_col -= n;
    console->cursor_col = console->cursor_col <= 0 ? 0 : console->cursor_col;
}

static void move_right(console_t *console) {
    ASSERT(console->curr_param_idx >= 0);
    int n = !console->esc_param[0] ? 1 : console->esc_param[0]; // if zero, then set to 1
    console->cursor_col += n;
    console->cursor_col = console->cursor_col >= console->disp_cols ?
                          console->disp_cols - 1 : console->cursor_col;
}

static void write_esc_seq(console_t *console, char ch) {
    if (ch >= '0' && ch <= '9') {
        int *param = &console->esc_param[console->curr_param_idx];
        *param = *param * 10 + ch - '0';
    } else if (ch == ';') {
        ASSERT (console->curr_param_idx + 1 < ESC_PARAM_MAX);
        console->curr_param_idx++;
    } else if (ch == 'm') { // actually switch would be better here, for the alphs
        set_font_style(console);
        console->write_state = CONSOLE_WRITE_NORMAL;
    } else if (ch == 'D') {
        move_left(console);
        console->write_state = CONSOLE_WRITE_NORMAL;
    } else if (ch == 'C') {
        move_right(console);
        console->write_state = CONSOLE_WRITE_NORMAL;
    } else if (ch == 'H') {
        move_cursor(console);
        console->write_state = CONSOLE_WRITE_NORMAL;
    } else if (ch == 'f') {
        console->write_state = CONSOLE_WRITE_NORMAL;
    } else if (ch == 'J') {
        erase_in_display(console);
        console->write_state = CONSOLE_WRITE_NORMAL;
    }
}

int console_write(tty_t *tty) {
    console_t *c = console_buf + tty->console_idx;

    int len = 0;
    mutex_lock(&c->mutex);
    while (1) {
        char ch;
        int err = fifo_get(&tty->ofifo, &ch);
        if (err < 0) {
            break;
        }
        sem_notify(&tty->osem);
        switch (c->write_state) {
            case CONSOLE_WRITE_NORMAL:
                write_normal_state(c, ch);
                break;
            case CONSOLE_WRITE_ESC:
                write_esc_state(c, ch);
                break;
            case CONSOLE_WRITE_SEQ:
                write_esc_seq(c, ch);
            default:
                break;
        }

        len++;
    }
    mutex_unlock(&c->mutex);

    if (curr_console_idx == tty->console_idx) {
        update_cursor_pos(c);
    }
    
    return len;
}

void console_close(int console) {

}

void console_select(int idx) {
    if (idx < 0 || idx >= CONSOLE_NUM) {
        log_printf("invalid console idx");
        return;
    }

    curr_console_idx = idx;
    console_t *console = &console_buf[idx];
    // if (!console->disp_base) { // only open initializes console
    //     console_init(idx);
    // }

    // this tells console hardware what's the memory position that
    // corresponds to the top leftmost position
    uint16_t pos = idx * console->disp_rows * console->disp_cols;
    outb(0x3D4, 0xC);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    outb(0x3D4, 0xD);
    outb(0x3D5, (uint8_t)(pos & 0xFF));

    update_cursor_pos(console);

    // char num = idx + '0';
    // show_char(console, num);
}