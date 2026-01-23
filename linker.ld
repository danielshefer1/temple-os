ENTRY(stage4_entry)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    /* Stage 4 (on disk) */
    . = 0x8E00;
    _start_of_kernel_file = .;

    .stage4 : {
        *stage4.o(.text)
    }

    /* LMA where kernel pieces are placed on disk */
    _kernel_LMA_start = .;

    /* Bootstrap placed at physical 1MB */
    . = 0x100000;
    _kernel_physical_start = .;
    .bootstrap : AT(_kernel_LMA_start) {
        *(.bootstrap)
        *(.bootstrap.text)
        *(.rodata.bootstrap*)
        *(.data.bootstrap*)
    }
    _bootstrap_size = SIZEOF(.bootstrap);

    /* Kernel VMA starts at 2GB + 1MB plus bootstrap size */
    . = 0x80100000 + _bootstrap_size;
    _kernel_VMA_start = .;

    . = ALIGN(4);
    _text_VMA_actual = .;

    _helpers_start = _kernel_LMA_start + _bootstrap_size + _text_VMA_actual - _kernel_VMA_start;
    .helpers : AT(_helpers_start) {
        *(.helpers)
    }

    _text_start_V = .;
    . = ALIGN(32);
    _text_start_V_actual = .;
    /* Compute properly aligned text start */
    _text_start = _helpers_start + SIZEOF(.helpers) + _text_start_V_actual - _text_start_V;
    .text : AT(_text_start) {
        *(.text)
        *(.rodata*)
    }

    _data_start_V = .;

    . = ALIGN(4K);

    _data_start_V_actual = .;
    _data_start = _text_start + SIZEOF(.text) + _data_start_V_actual - _data_start_V;
    .data : AT(_data_start) {
        *(.data)
    }

    /* End of data that must be copied from disk */
    _kernel_VMA_data_end = .;

    /* BSS (does not take space on disk) */
    _bss_start = _data_start + SIZEOF(.data) + _kernel_physical_start - _kernel_LMA_start;
    .bss : AT(_data_start + SIZEOF(.data)) {
        *(.bss)
        *(COMMON)
    }
    _bss_end = _bss_start + SIZEOF(.bss);
    _bss_size = SIZEOF(.bss);
    _kernel_VMA_end = .;
}

/* Calculations used by the bootloader / loader */
_binary_size_text_data = _kernel_VMA_data_end - _kernel_VMA_start + _bootstrap_size;
_text_size = SIZEOF(.text);


__end_of_file_phys = _data_start + SIZEOF(.data);
__file_size_bytes = __end_of_file_phys - _kernel_LMA_start;
__kernel_size_bytes = _kernel_VMA_end - _kernel_VMA_start + SIZEOF(.bootstrap);

__sector_size = 512;
__page_size = 1024 * 4;

__total_sectors = (__file_size_bytes + __sector_size - 1) / __sector_size;
__total_pages = (__kernel_size_bytes + __page_size - 1 - __sector_size) / __page_size;