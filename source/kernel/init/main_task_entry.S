// here we're trying to separate the main task code from kernel code 
    .text
    .global main_task_entry // export this
    .extern main_task
main_task_entry:
    mov %ss, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    jmp main_task // only add selector in jmp when crossing process or from task to kernel
