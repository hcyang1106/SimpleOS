#ifndef LOADER_H
#define LOADER_H

#include "comm/boot_info.h"
#include "comm/types.h"
#include "comm/cpu_instr.h"

void protect_mode_entry(void);

extern boot_info_t boot_info; // boot_info exist in loader_16.c, we want loader_32.c to be able to use it

#endif
