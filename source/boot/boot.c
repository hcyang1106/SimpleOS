// ask compiler to compile 16 bit instructions
__asm__(".code16gcc");

#include "boot.h"

// jumps to _start because we can control the address of _start
// can't control the address of c functions
#define LOADER_START_ADDR 0x8000

void boot_entry(void) {
    ((void (*)(void))LOADER_START_ADDR)(); // jumps to loader code
}

// function pointer example:
// void sayHello() {
//     printf("Hello, world!\n");
// }

// int main() {
//     void (*fp)() = &sayHello;
//     (*fp)(); 
//     return 0;
// }

// function returns a function pointer:
// int (*getOperation(char op))(int, int) {
//     switch (op) {
//         case '+': return &add;
//         case '-': return &subtract;
//         case '*': return &multiply;
//         case '/': return &divide;
//         default: return NULL;
//     }
// }

