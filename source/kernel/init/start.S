    .text
    .extern kernel_init
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
    jmp .