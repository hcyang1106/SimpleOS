// my question is why compiling applib doesn't gen error,
// because main is not a symbol under applib
// to my understanding, this compiles into a library (not executable)
// so without "main" under applib is okay
// linker only makes sure when "linking executable", all symbols exist
#include "comm/types.h"
#include <stdlib.h>

int main(int argc, char **argv); 

void cstart(int argc, char**argv) {
    extern uint8_t __bss_start__, __bss_end__;
    uint8_t *start = &__bss_start__;
    while (start < &__bss_end__) {
        *start = 0;
        start++;
    }
    
    exit(main(argc, argv));
    // main(argc, argv);
}