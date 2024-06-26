#ifndef TYPES_H
#define TYPES_H

// useful links
// https://stackoverflow.com/questions/2331751/does-the-size-of-an-int-depend-on-the-compiler-and-or-processor
// https://stackoverflow.com/questions/32127689/how-is-word-size-in-computer-related-to-int-or-long
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

#endif

// char is gauranteed to be 1 byte, and whether it is signed or not is undefined
// why use unsigned to represent bytes:
// comparing and shifting might have different results (signed vs unsigned)

// comparing:

// char c[5];
// c[0] = 0xff;
// /*blah blah*/
// if (c[0] == 0xff) 
// {
//     // unsigned
//     printf("good\n");
// }
// else
// {
//     // signed
//     printf("bad\n");
// }

// shifting:

// char c[5], d[5];
// c[0] = 0xF0;
// c[1] = 0xA4;
// c[2] = 0xAD;
// c[3] = 0xA2;
// c[4] = '\0';
// c[0] >>= 1; // leads to different results

