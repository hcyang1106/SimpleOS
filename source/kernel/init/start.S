#include "os_cfg.h"

    .text
    .extern kernel_init
    .extern init_main
    .global _start

// void _start(boot_info_t *boot_info)
// in start, we get the parameter using ebp
_start: 
    push %ebp
    mov %esp, %ebp
    mov 0x8(%ebp), %eax // ebp+4 is return address, so ebp+8 here
    push %eax // parameter for void kernel_init(boot_info_t*)

    // alternatively, we can just write mov 4(%esp), %eax to replace the above instructions
    // because we are not jumping back to loader
    // esp points to return address, so esp+4 is the parameter

    call kernel_init
    
    jmp $KERNEL_SELECTOR_CS, $gdt_reload

gdt_reload:
	mov $KERNEL_SELECTOR_DS, %ax
	mov %ax, %ds
    mov %ax, %ss
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

	// stack reset
	mov $(stack + KERNEL_STACK_SIZE), %esp

	jmp init_main

    .bss
.comm stack, KERNEL_STACK_SIZE // allocate a variable

// we need to write the exception function in assembly
// because we need iret at the end (C uses ret command)
.macro exception_handler name num with_error_code
    .text
    .extern do_handler_\name // in assembly we don't need .h file to use it
    .global exception_handler_\name
exception_handler_\name:
    // "cpu" preserves EIP, CS, EFLAGS and we should preserve other registers ourselves
    .if  \with_error_code == 0
        push $0
    .endif

    push $\num
    pusha
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

    add $(2*4), %esp // iret only pops EIP, CS, and EFLAGS (without popping the error code)
    iret


    // what is stored in stack (from high to low):
    // EIP, CS, EFLAGS (pushed by cpu)
    // error code (pushed by cpu or us)
    // other regs pushed by us
    // a "pointer" pointing to the address of last stored reg
    // return address (because of call function)

.endm

exception_handler unknown, -1, 0
exception_handler divider, 0, 0
exception_handler Debug, 1, 0
exception_handler NMI, 2, 0
exception_handler breakpoint, 3, 0
exception_handler overflow, 4, 0
exception_handler bound_range, 5, 0
exception_handler invalid_opcode, 6, 0
exception_handler device_unavailable, 7, 0
exception_handler double_fault, 8, 1
exception_handler invalid_tss, 10, 1
exception_handler segment_not_present, 11, 1
exception_handler stack_segment_fault, 12, 1
exception_handler general_protection, 13, 1
exception_handler page_fault, 14, 1
exception_handler fpu_error, 16, 0
exception_handler alignment_check, 17, 1
exception_handler machine_check, 18, 0
exception_handler smd_exception, 19, 0
exception_handler virtual_exception, 20, 0

exception_handler timer, 0x20, 0
exception_handler kbd, 0x21, 0
exception_handler disk_primary, 0x2E, 0


    .text
    .global simple_switch
    // simple_switch(&from , to)
    // why do we need to store the stack pointer in task_t?
    // so that one task could access another task's stack pointer (in parameter, to->stack)
simple_switch:
    mov 4(%esp), %eax // pointer to pointer to stack of "from"
    mov 8(%esp), %edx // pointer to stack of "to"

    // pushes the regs of "from" task into "from" stack
    push %ebp
    push %ebx
    push %esi
    push %edi
    // save esp reg into task_t
    mov %esp, 0(%eax)
    

    // switches to the stack of "to" task
    mov %edx, %esp
    //pop the regs out
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp

    ret

    .global syscall_handler
    .extern do_handler_syscall
syscall_handler:
    pusha
    push %ds
    push %es
    push %fs
    push %gs
    pushf # for eflags

    push %esp
    # a function that carries one param
    # the param is esp, which stores the address of syscall_args_t
    call do_handler_syscall
    add $4, %esp

    popf
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa


    # simply using ret here will only pop out the eip,
    # privilege level will still be zero
    # this is used to tell cpu how many "bytes" should be popped (the params)
    # five 4 bytes params
    # after this instruction both level 0 stack and level 3 stack esp will move up
    # level 3 move up 4*5 and level 0 move up 4*5 and more (also popped eip, cs, ss, esp...)
    # when doing fork, the child process will not go through this,
    # so we need to manually move up its esp
    retf $(4 * 5) 



// Caller‐save registers are responsibility of the caller
// • Caller‐save register values saved only if used "after call/return"
// • The callee function can use caller‐saved registers
// Callee‐save register are the responsibility of the callee
// • Values must be saved by callee "before they can be used"
// • Caller can assume that these registers will be restored

// example of caller‐saved and callee-saved reg
// source: https://www.cs.princeton.edu/courses/archive/spr17/cos217/lectures/15_AssemblyFunctions.pdf
// b is stored in RSI
# long absadd(long a, long b)
//absadd:
//   pushq %r13 # Save R13, use for absA // callee-saved
//   pushq %r14 # Save R14, use for absB
//    pushq %r15 # Save R15, use for sum
//
//   # absA = labs(a)
//    pushq %rsi # Save RSI (b) // caller-saved
//    call labs
//    movq %rax, %r13
//    popq %rsi # Restore RSI
//
//    # absB += labs(b)
//    movq %rsi, %rdi
//    call labs
//    movq %rax, %r14
//
//    # sum = absA + absB
//    movq %r13, %r15
//    addq %r14, %r15
//
//    # return sum
//    movq %r15, %rax
//
//    popq %r15 # Restore R15
//    popq %r14 # Restore R14
//    popq %r13 # Restore R13
//
//    ret