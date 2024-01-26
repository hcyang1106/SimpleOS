/**
 * This code segment (boot) is initially stored in the first sector of the disk.
 * It is then loaded to memory address 0x7c00 after the computer starts.
 * Boot is reponsible for loading the "loader".
 */
	#include "boot.h"

  	.code16 // x86 CPU starts in real mode, which operates on 16 bits architecture
 	.text
	.global _start // .global makes it accessible by other files 
_start:
	// set segment registers to zero
	mov $0, %ax
	mov %ax, %ds
	mov %ax, %ss
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	// set the memory before 0x7c00 as stack space
	mov $_start, %esp

	// local jump
	jmp .

	.section boot_end, "ax"
boot_sig: .byte 0x55, 0xaa
