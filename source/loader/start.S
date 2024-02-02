  	.code16 // x86 CPU starts in real mode, which operates on 16 bits architecture
 	.text
	.global _start // .global makes it accessible by other files
	.extern loader_entry
_start: // memory address 0x8000
	jmp loader_entry

	.code32
	.text
	.global protect_mode_entry
	.extern load_kernel
protect_mode_entry:
	mov $16, %ax
	mov %ax, %ds
	mov %ax, %ss
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	jmp $8, $load_kernel
