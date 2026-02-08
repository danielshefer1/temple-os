ENTRY(kmain)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS
{
    _kernel_LMA_start = 0x200000;
    . = 0xFFFFFFFF80200000;
    _kernel_VMA_start = .;

    /* Map .helpers at the very start of the LMA */
    .helpers : AT(_kernel_LMA_start) {
        *(.helpers)
    }

    /* For every subsequent section, calculate LMA based on its VMA offset */
    /* LMA = Start_LMA + (Current_VMA - Start_VMA) */

    .trampoline : AT(_kernel_LMA_start + (ADDR(.trampoline) - _kernel_VMA_start)) {
        *(.trampoline)
    }

    .text : AT(_kernel_LMA_start + (ADDR(.text) - _kernel_VMA_start)) {
        *(.text)
        *(.rodata*)
        *(.eh_frame) /* Explicitly handle this to avoid overlaps */
    }

    __text_size = . - _kernel_VMA_start;

    /* Use ALIGN inside the section definition so VMA and LMA both move */
    .data : AT(_kernel_LMA_start + (ADDR(.data) - _kernel_VMA_start)) {
        . = ALIGN(2M); 
        *(.data)
    }

    /* Use a symbol to mark the end of the data to be loaded into RAM */
    __end_of_file_vma = .;
    __end_of_file_phys = _kernel_LMA_start + (__end_of_file_vma - _kernel_VMA_start);

    .bss : AT(__end_of_file_phys) {
        _bss_start = . - _kernel_VMA_start + _kernel_LMA_start;
        *(.bss)
        *(COMMON)
        . = ALIGN(4K);        
        _stack_bottom = .;
        . += 16384;      
        _stack_top = .;
        _bss_end = . - _kernel_VMA_start + _kernel_LMA_start;
    }
    _kernel_VMA_end = .;
}

__file_size_bytes = __end_of_file_phys - _kernel_LMA_start;
__kernel_size_bytes = _kernel_VMA_end - _kernel_VMA_start;

__sector_size = 512;
__page_size = 4096;

__kernel_sectors = (__file_size_bytes + __sector_size - 1) / __sector_size;
__total_pages = (__kernel_size_bytes + __page_size - 1) / __page_size;