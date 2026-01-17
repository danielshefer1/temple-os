ENTRY(main)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    /* Dummy starting point */
    . = 0x1000;
    .text : AT (.) {
        *(.text.entry)
        *(.text)
        *(.rodata*)
    }
    . = ALIGN(4K);
    .data : AT(.) {
        *(.data)
    }
    .bss : AT(.) {
        *(.bss);
        *(COMMON);
    }
}