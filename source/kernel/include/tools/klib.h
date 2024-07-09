#ifndef KLIB_H
#define KLIB_H

#include <stdarg.h>
#include "comm/types.h"

static inline uint32_t down(uint32_t size, uint32_t unit) {
    // return size & ~(unit - 1);
    return size / unit * unit;
}

static inline uint32_t up(uint32_t size, uint32_t unit) {
    // return size + (unit - 1) & ~(unit - 1);
    return (size + unit - 1) / unit * unit;
}

// when we won't modify the value char* points to, use const 
void kernel_strcpy(char *dest, const char *src);
void kernel_strncpy(char *dest, const char *src, int n);
int kernel_strncmp(const char *s1, const char *s2, int n);
int kernel_strlen(const char *s); 

void kernel_memcpy(void *dest, const void *src, int n);
void kernel_memset(void *dest, uint8_t v, int n);
int kernel_memcmp(const void *d1, const void *d2, int n);

void kernel_vsprintf(char *buf, const char *fmt, va_list args);
void kernel_sprintf(char *buf, const char *fmt, ...);
void kernel_itoa(char *buf, int num, int base);

#ifndef RELEASE

#define ASSERT(expr) if(!(expr)) panic(__FILE__, __LINE__, __func__, #expr) // # turns expr into string
// only use this prototype when defined RELEASE
void panic(const char *file, int line, const char *func, const char *cond);

#else
#define ASSERT(expr) ((void)0)

#endif

#endif