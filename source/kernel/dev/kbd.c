#include "dev/kbd.h"
#include "cpu/cpu.h"
#include "tools/log.h"
#include "comm/types.h"
#include "comm/cpu_instr.h"
#include "dev/tty.h"

static kbd_state_t kbd_state;

static const key_map_t map_table[] = {
        [0x2] = {'1', '!'},
        [0x3] = {'2', '@'},
        [0x4] = {'3', '#'},
        [0x5] = {'4', '$'},
        [0x6] = {'5', '%'},
        [0x7] = {'6', '^'},
        [0x08] = {'7', '&'},
        [0x09] = {'8', '*' },
        [0x0A] = {'9', '('},
        [0x0B] = {'0', ')'},
        [0x0C] = {'-', '_'},
        [0x0D] = {'=', '+'},
        [0x0E] = {'\x7f', '\x7f'},
        [0x0F] = {'\t', '\t'},
        [0x10] = {'q', 'Q'},
        [0x11] = {'w', 'W'},
        [0x12] = {'e', 'E'},
        [0x13] = {'r', 'R'},
        [0x14] = {'t', 'T'},
        [0x15] = {'y', 'Y'},
        [0x16] = {'u', 'U'},
        [0x17] = {'i', 'I'},
        [0x18] = {'o', 'O'},
        [0x19] = {'p', 'P'},
        [0x1A] = {'[', '{'},
        [0x1B] = {']', '}'},
        [0x1C] = {'\n', '\n'},
        [0x1E] = {'a', 'A'},
        [0x1F] = {'s', 'S'},
        [0x20] = {'d',  'D'},
        [0x21] = {'f', 'F'},
        [0x22] = {'g', 'G'},
        [0x23] = {'h', 'H'},
        [0x24] = {'j', 'J'},
        [0x25] = {'k', 'K'},
        [0x26] = {'l', 'L'},
        [0x27] = {';', ':'},
        [0x28] = {'\'', '"'},
        [0x29] = {'`', '~'},
        [0x2B] = {'\\', '|'},
        [0x2C] = {'z', 'Z'},
        [0x2D] = {'x', 'X'},
        [0x2E] = {'c', 'C'},
        [0x2F] = {'v', 'V'},
        [0x30] = {'b', 'B'},
        [0x31] = {'n', 'N'},
        [0x32] = {'m', 'M'},
        [0x33] = {',', '<'},
        [0x34] = {'.', '>'},
        [0x35] = {'/', '?'},
        [0x39] = {' ', ' '},
};

void kbd_init(void) {
    static int inited = 0;
    if (!inited) {
        kbd_state.lshift = kbd_state.rshift = 0;
        irq_install(IRQ1_KEYBOARD, (irq_handler_t)exception_handler_kbd);
        irq_enable(IRQ1_KEYBOARD);

        inited = 1;
    } 
}

// pressing, when pressed 7th bit is zero
// make/break code
static inline int is_make_code(uint8_t code) {
    return !(code & 0x80);
}

// make 7th bit zero
static inline char get_key(uint8_t code) {
    return code & 0x7f;
}

static inline int state_is_shift(kbd_state_t *state) {
    if (state->lshift || state->rshift) {
        return 1;
    }

    return 0;
}

static void do_fx_key(char key) {
    int idx = key - KEY_F1;
    if (kbd_state.ctrl) {
        tty_select(idx);
    }
}

// for keys that only have one value
static void do_normal_key(uint8_t code) {
    int is_make = is_make_code(code);
    char key = get_key(code);
    switch (key) {
        case KEY_RSHIFT:
            kbd_state.rshift = is_make;
            break;
        case KEY_LSHIFT:
            kbd_state.lshift = is_make;
            break;
        case KEY_CAPS: 
            // make it do nothing for now
            break;
        case KEY_CTRL:
            kbd_state.ctrl = is_make;
            break;
        case KEY_F1:
        case KEY_F2:
        case KEY_F3:
        case KEY_F4:
        case KEY_F5:
        case KEY_F6:
        case KEY_F7:
        case KEY_F8:
            do_fx_key(key);
            break;
        case KEY_F9:
        case KEY_F10:
        case KEY_F11:
        case KEY_F12:
            break;
        default:
            if (is_make) {
                char out;
                if (state_is_shift(&kbd_state)) {
                    out = map_table[key].func;
                } else {
                    out = map_table[key].normal;
                }

                // if (kbd_state.caps_lock) {
                //     if ((out >= 'A') && (out <= 'Z')) {
                //         out = out - 'A' + 'a';
                //     } else if ((out >= 'a') && (out <= 'z')) {
                //         out = out - 'a' + 'A';
                //     }
                // }

                // log_printf("key: %c", out);
                tty_in(out);

            }
            break;
    }
}

static void do_e0_key(int code) {
    // this is used to deal with right ctrl and alt
    // but mac does not have these two 
    // so leave it empty here
}

static void do_e1_key(int code) {
}

void do_handler_kbd(exception_frame_t *frame) {
    static enum {
        NORMAL,
        BEGIN_E0,
        BEGIN_E1,
    }recv_state = NORMAL;

    uint32_t status = inb(KBD_PORT_STAT);
    if ((!status & KBD_STAT_RECV_READY)) {
        pic_send_eoi(IRQ1_KEYBOARD);
        return;
    }

    uint8_t code = inb(KBD_PORT_DATA);
    // do_normal_key(code);

    // this tells 8259 current request has finished
    pic_send_eoi(IRQ1_KEYBOARD);

    if (code == KEY_E0) {
        recv_state = BEGIN_E0;
    } else if (code == KEY_E1) {
        recv_state = BEGIN_E1;
    } else {
        switch (recv_state) {
            case NORMAL:
                do_normal_key(code);
                break;
            case BEGIN_E0:
                do_e0_key(code);
                recv_state = NORMAL;
                break;
            case BEGIN_E1:
                do_e1_key(code);
                recv_state = NORMAL;
                break;
            default:
                break;
        }
    }
}
