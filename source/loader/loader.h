#ifndef LOADER_H
#define LOADER_H

#include "comm/boot_info.h"
#include "comm/types.h"
#include "comm/cpu_instr.h"

void protect_mode_entry(void);

// boot_info exist in loader_16.c, we want loader_32.c to be able to use it
// static variables cannot be declared extern
// https://stackoverflow.com/questions/31244540/can-a-static-variable-be-declared-extern-in-c
extern boot_info_t boot_info; 

#endif
