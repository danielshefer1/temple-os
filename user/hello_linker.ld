ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS {
    . = 0;
    .text     : { *(.text*) }
    .rodata   : { *(.rodata*) }
    .data     : { *(.data*) }
    .bss      : { *(.bss*) *(COMMON) }
    .dynamic  : { *(.dynamic) }
    .rela.dyn : { *(.rela.dyn) *(.rela.*) }
    .dynsym   : { *(.dynsym) }
    .dynstr   : { *(.dynstr) }
    .gnu.hash : { *(.gnu.hash) }
    .hash     : { *(.hash) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.interp) }
}
