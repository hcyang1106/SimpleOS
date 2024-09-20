    .text
    .global _start
    .extern cstart
_start: 
# this address will not be 0x81000000,
# which means the entry will not always be the smallest address
    mov %ss, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    jmp cstart # can't jump to main because of different project