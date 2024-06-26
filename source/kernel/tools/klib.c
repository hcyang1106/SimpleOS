#include "tools/klib.h"
#include "tools/log.h"
#include "comm/cpu_instr.h"

// useful link about null pointers: https://stackoverflow.com/questions/76219480/is-every-null-pointer-constant-a-null-pointer
// Conversion of a null pointer to another pointer type yields a null pointer of that type. Any two null pointers shall compare equal.

// void*: Before dereferencing, the data that a void pointer points to must undergo proper typecasting to the correct type to be used


void kernel_strcpy(char *dest, const char *src) { 
    // char* means we will modify the value it points to
    // this function doesn't care about the length dest pointer points to
    // remember the case of null pointer
    if (!dest || !src) {
        return;
    }

    char *dest_start = dest;

    while (*src) { 
        *dest = *src;
        dest++;
        src++;
    }

    *dest = '\0';

    return;
}

// copy n bytes, if src is less than n bytes then fill dest with '\0's
void kernel_strncpy(char *dest, const char *src, int n) {
    if (!dest || !src) {
        return;
    }

    int i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    
    if (dest[i-1] != '\0') {
        dest[i] = '\0';
    }
    
    return;
}

// does not check null pointers
// returns -1 if s1 < s2
// returns 0 if s1 == s2
// returns 1 if s1 > s2
int kernel_strncmp(const char *s1, const char *s2, int n) {
    while (*s1 && *s2 && (*s1 == *s2) && n) {
        s1++;
        s2++;
        n--;
    }

    if (n == 0 || *s1 == *s2) {
        return 0;
    }

    if (*s1 > *s2) {
        return 1;
    }

    return -1;
}

// strlen vs. sizeof => length of a string / length of bytes
int kernel_strlen(const char *s) {
    if (!s) {
        return 0;
    }
    // char *p = s; this line is strongly not recommended because it assigns const char* to char*
    // which might modify the value
    int count = 0;
    while(*s) {
        count++;
        s++;
    }

    return count;
} 

void kernel_memcpy(void *dest, const void *src, int n) {
    if (!dest || !src || !n) {
        return;
    }

    uint8_t *d = dest;
    const uint8_t *s = src;

    for (int i = 0; i < n; i++) {
        *d = *s;
        d++;
        s++;
    }

    return;
}

void kernel_memset(void *dest, uint8_t v, int n) {
    if (!dest || !n) {
        return;
    }

    uint8_t *d = dest;
    for (int i = 0; i < n; i++) {
        *d = v;
        d++;
    }
}

// doesn't check null pointer
int kernel_memcmp(const void *d1, const void *d2, int n) {
    if (n == 0) {
        return 0;
    }

    const uint8_t *b1 = d1;
    const uint8_t *b2 = d2;

    for (int i = 0; i < n; i++) {
        if (*b1 < *b2) {
            return -1;
        }
        if (*b1 > *b2) {
            return 1;
        }
    }

    return 0;
}

void kernel_vsprintf(char *buf, const char *fmt, va_list args) {
    enum {NORMAL, READ_FMT} state = NORMAL;

    while (*fmt) {
        switch (state)
        {
        case NORMAL:
            if (*fmt == '%') {
                state = READ_FMT;
            } else {
                *buf++ = *fmt; 
            }
            break;
        case READ_FMT:
            if (*fmt == 's') {
                const char *str = va_arg(args, char*);
                kernel_strcpy(buf, str);
                buf += kernel_strlen(str);
            } else if (*fmt == 'd') {
                int num = va_arg(args, int);
                // I thought the itoa prototype being like this (below)
                // however it is impossible that you don't pass a pointer
                // into the function but you get a pointer back
                // because if this is the case the pointer passing back
                // will be a local variable and will disappear after the function ends
                // const char *str = kernel_itoa(num, 10);
                // updated: another choice, use return malloced pointer?
                kernel_itoa(buf, num, 10);
                buf += kernel_strlen(buf);
            } else if (*fmt == 'x') {
                int num = va_arg(args, int);
                kernel_itoa(buf, num, 16);
                buf += kernel_strlen(buf);
            } else if (*fmt == 'c') {
                const char ch = va_arg(args, int); // warn that this should be int
                *buf++ = ch;
            }
            // add assert?
            state = NORMAL;
            break;
        default:
            break;
        }
        
        fmt++;
    }
    
    *buf = '\0';
}

void kernel_sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt); // the second argument is to pass "the last parameter" that is not "variable arguments"
    kernel_vsprintf(buf, fmt, args);
    va_end(args);
}

// only base 10 uses '-' to represent negative numbers
// others use two's complement
// num = -num; wrong implementation if num is 0x80000000 (smallest int), doing this will cause overflow
// for base 2, 8, 16, we can convert it directly
// however for base 10 positive and negative numbers have different representations
// so we have to do it separately
void kernel_itoa(char *buf, int num, int base) {
    if (base != 2 && base != 8 && base != 10 && base != 16) {
        return;
    }

    char str[64];
    char *p = str;

    // only base 10 uses this representation
    if (num < 0 && base == 10) {
        *buf++ = '-';
        static const char *dict = "FEDCBA9876543210";
        do {
            *p++ = dict[num%base + 15];
            num = num / base;
        } while (num); // when num is zero, we should still do it once
    
    } else {
        static const char *udict = "0123456789ABCDEF";
        uint32_t unum = (uint32_t)num;
        do {
            *p++ = udict[unum%base];
            unum = unum / base;
        } while (unum); // when unum is zero, we should still do it once
    }

    p--;
    int iters = kernel_strlen(str);
    while (iters--) {
        *buf++ = *p--;
    }

    *buf = '\0';
}

void panic(const char *file, int line, const char *func, const char *cond) {
    log_printf("Assert Failed! %s", cond);
    log_printf("file: %s\nline: %d\nfunc: %s\n", file, line, func);
    for (;;) {
        hlt();
    }
}