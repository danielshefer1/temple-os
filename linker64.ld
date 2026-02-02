ENTRY(kmain)
OUTPUT_FORMAT(elf64-i386)

SECTIONS
{
    _kernel_LMA_start = 0x200000 /* 2MB - Physical Address */
    . = 0xFFFFFFFF80000000;
    _kernel_VMA_start = .;

    .helpers { *(.helpers) }

    .trampoline { *(.trampoline) }

    
    .text : ALIGN(4)  {
        *(.text)
        *(.rodata*)
    }

    .data : ALIGN(4K) { *(.data) }

    __end_of_file_phys = .;


    _bss_start = . - _kernel_VMA_start + _kernel_LMA_start;
    .bss {
        *(.bss)
        *(COMMON)
    }
    _bss_end = _bss_start + SIZEOF(.bss);
    _kernel_VMA_end = .;
}


__file_size_bytes = __end_of_file_phys - _kernel_VMA_start;
__kernel_size_bytes = _kernel_VMA_end - _kernel_VMA_start;

__sector_size = 512;
__page_size = 4096;

__kernel_sectors = (__file_size_bytes + __sector_size - 1) / __sector_size;
__total_pages = (__kernel_size_bytes + __page_size - 1) / __page_size;