OUTPUT_FORMAT(elf32-i386)

ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R + X */
    data PT_LOAD FLAGS(6);   /* R + W */
}

SECTIONS {
    . = 0x10000;

    .text : {
        *(.text)
    } :text

    .rodata : {
        *(.rodata*)
    } :text

    .data : {
        *(.data)
    } :data

    .bss : {
        *(COMMON)
        *(.bss)
    } :data

kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
    }
}
