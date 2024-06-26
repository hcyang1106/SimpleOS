#ifndef TIME_H
#define TIME_H

#define PIT_OSC_FREQ                1193182 // timer ticks this many times per second

#define PIT_CHANNEL0_DATA_PORT       0x40
#define PIT_COMMAND_MODE_PORT        0x43

#define PIT_CHANNLE0                (0 << 6) // there are 3 channels and the first one is used
#define PIT_LOAD_LOHI               (3 << 4)
#define PIT_MODE3                   (3 << 1)

void time_init(void);

#endif