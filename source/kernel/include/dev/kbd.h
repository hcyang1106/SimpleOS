#ifndef KBD_H
#define KBD_H

#include "comm/types.h"

#define KEY_RSHIFT 0x36
#define KEY_LSHIFT 0x2A
#define KEY_CAPS 0x3A

#define KEY_CTRL 0x1D
#define KEY_E0 0xE0
#define KEY_E1 0xE1

#define KEY_F1			(0x3B)
#define KEY_F2			(0x3C)
#define KEY_F3			(0x3D)
#define KEY_F4			(0x3E)
#define KEY_F5			(0x3F)
#define KEY_F6			(0x40)
#define KEY_F7			(0x41)
#define KEY_F8			(0x42)
#define KEY_F9			(0x43)
#define KEY_F10			(0x44)
#define KEY_F11			(0x57)
#define KEY_F12			(0x58)

typedef struct _key_map_t {
    uint8_t normal;
    uint8_t func;
}key_map_t;

// each field is one bit
typedef struct _kbd_state_t {
    int lshift: 1;
    int rshift: 1;
    // int caps_lock: 1; wierd behavior so remove temporarily
    int ctrl: 1;
}kbd_state_t;

#define KBD_PORT_DATA 0x60
#define KBD_PORT_STAT 0x64
#define KBD_PORT_CMD  0x64

#define KBD_STAT_RECV_READY (1 << 0)

void kbd_init(void);
void exception_handler_kbd(void);

#endif