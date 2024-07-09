# address before text, data, bss, these three areas are readable and writable
# address of text and rodata are readable not writable
SECTIONS {
    . = 0x10000; # 64KB

    PROVIDE(s_text = .);
    .text : {
        # merge the text sections of all object files
        *(EXCLUDE_FILE(*main_task*) .text)
    }

    .rodata : {
        *(EXCLUDE_FILE(*main_task*) .rodata)
    }
    PROVIDE(e_text = .);

    # align is used to prevent rodata and data having overlapped page
    # (due to up/down function)
    . = ALIGN(4096);
    PROVIDE(s_data = .);
    .data : {
        *(EXCLUDE_FILE(*main_task*) .data)
    }

    .bss : {
        *(EXCLUDE_FILE(*main_task*) .bss)
    }
    e_data = .; # is a virtual address, but below 1MB, virtual equals to physical

    . = 0x80000000;
    PROVIDE(s_main_task = LOADADDR(.main_task));
    .main_task : AT(e_data){                                  # a space must exist between "main_task" and ":"!
        *main_task_entry*(.text .rodata .bss .data) # .S file
        *main_task*(.text .rodata .bss .data)       # .c file
    }
    PROVIDE(e_main_task = LOADADDR(.main_task) + SIZEOF(.main_task)); # loadaddr returns the physical address

    # define an address right behind bss, and use PROVIDE keyword to make
    # it accessible by c
    PROVIDE(mem_free_start = e_main_task);
}

# text: section where instructions locates at
# rodata: stores constants (including strings)
# data: global or static "initialized" variables
# bss: global or static "uninitialized" variables
# local variables are stored in stack area
# elf is a file format, .o, .bin... are in this format

# compiler address: the address "relative" to the start of the object file
# logical address: after linking, it relocates and become "logical address"
# logical address equals to virtual (linear) address because segment regs are zeroes