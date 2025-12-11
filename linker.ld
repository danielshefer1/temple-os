/* linker.ld */
ENTRY(stage3_entry)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    /* --- Stage 3: Starts at 0x8600 --- */
    . = 0x8600;

    .stage3 : {
        stage3.o(.text) 
    }

    /* * Calculate where the Kernel code is physically located 
     * in the binary file (Load Memory Address - LMA) 
     */
    _kernel_LMA_start = .;

    /* --- C Kernel: Starts at 1MB (0x100000) --- */
    . = 0x100000;
    
    _kernel_VMA_start = .;

    /* Link .text to run at 1MB, but load it at _kernel_LMA_start */
    .text : AT(_kernel_LMA_start)
    {
        *(.text)
        *(.rodata*)
    }

    .data : AT(_kernel_LMA_start + SIZEOF(.text))
    {
        *(.data)
    }

    .bss : AT(_kernel_LMA_start + SIZEOF(.text) + SIZEOF(.data))
    {
        *(.bss)
        *(COMMON)
    }
    
    _kernel_VMA_end = .;

    __kernel_size_bytes = _kernel_VMA_end - _kernel_VMA_start;

    __sector_size = 512;
    __total_sectors = (__kernel_size_bytes + __sector_size - 1) / __sector_size;
}