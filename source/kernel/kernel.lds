SECTIONS {
    . = 0x10000;

    .text : {
        # merge the text sections of all object files
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}

# text: section where instructions locates at
# rodata: stores constants (including strings)
# data: global or static "initialized" variables
# bss: global or static "uninitialized" variables
# local variables are stored in stack area
# elf is a file format, .o, .bin... are in this format