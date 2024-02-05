#include "os_cfg.h"

    .text
    .extern kernel_init
    .extern init_main
    .global _start
_start: // void _start(boot_info_t *boot_info)
    // use assembly to implement a function call of void kernel_init(boot_info_t*)
    push %ebp
    mov %esp, %ebp
    mov 0x8(%ebp), %eax
    push %eax

    // alternatively, we can just write mov 4(%esp), %eax
    // because we are not jumping back to loader

    call kernel_init
    
    jmp $KERNEL_SELECTOR_CS, $stack_reset

stack_reset:
    mov $(stack + KERNEL_STACK_SIZE), %esp
    jmp init_main

    .bss
.comm stack, KERNEL_STACK_SIZE // allocate a variable

// we need to write the exception function in assembly
// because we need iret at the end (C uses ret command)
.macro exception_handler name num with_error_code
    .text
    .extern do_handler_\name
    .global exception_handler_\name
exception_handler_\name:
    .if  \with_error_code == 0
        push $0
    .endif

    push $\num
    pusha // cpu preserves EIP, CS, EFLAGS and we should preserve other registers ourselves
    push %ds
    push %es
    push %fs
    push %gs

    push %esp

    call do_handler_\name

    add $(1*4), %esp // move up one, otherwise esp would be popped to the following register

    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa // cpu preserves EIP, CS, EFLAGS and we should preserve other registers ourselves

    add $(2*4), %esp // iret only pops EIP, CS, and EFLAGS (without popping the error_code)
    iret

.endm

exception_handler unknown, -1, 0
exception_handler divider, 0, 0