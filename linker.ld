ENTRY(stage3_entry)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    /* --- Stage 3: Starts at 0x8600 --- */
    . = 0x8600;
    _start_of_kernel_file = .; /* Mark start of file on disk */

    .stage3 : {
        /* Use wildcard to be safe against filename changes */
        *stage3.o(.text) 
        *(.text.stage3_entry) 
    }

    /* LMA calculation for C Kernel */
    _kernel_LMA_start = .;

    /* --- C Kernel VMA: Starts at 1MB --- */
    . = 0x100000;
    _kernel_VMA_start = .;

    .text : AT(_kernel_LMA_start)
    {
        *(.text)
        *(.rodata*)
    }

    .data : AT(_kernel_LMA_start + SIZEOF(.text))
    {
        *(.data)
    }

    /* Mark the end of the DATA (Stuff we must copy from disk) */
    _kernel_VMA_data_end = .;

    .bss : AT(_kernel_LMA_start + SIZEOF(.text) + SIZEOF(.data))
    {
        _bss_start = .;
        *(.bss)
        *(COMMON)
        _bss_end = .;
    }
    
    _kernel_VMA_end = .;

    /* --- Calculation Fixes --- */
    
    /* 1. Size to copy from disk (Text + Data only, NO BSS) */
    _binary_size_text_data = _kernel_VMA_data_end - _kernel_VMA_start;

    /* 2. Total sectors to load (Stage3 + C Kernel Text/Data) */
    /* We calculate physical file end address minus start address */
    __end_of_file_phys = _kernel_LMA_start + _binary_size_text_data;
    __file_size_bytes = __end_of_file_phys - _start_of_kernel_file;

    __sector_size = 512;
    __total_sectors = (__file_size_bytes + __sector_size - 1) / __sector_size;
}