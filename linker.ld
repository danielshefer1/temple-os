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

    . = 0x100000;
    _kernel_physical_start = .;
    .bootstrap : AT(_kernel_LMA_start)
    {
        *(.bootstrap)
        *(.bootstrap.text)
        *(.rodata.bootstrap*)
        *(.data.bootstrap*)
    }
    _bootstrap_size = SIZEOF(.bootstrap);

    /* --- C Kernel VMA: Starts at 2GB + 1MB --- */
    . = 0x80100000;
    _kernel_VMA_start = .;

    .text : AT(_kernel_LMA_start + SIZEOF(.bootstrap))
    {
        *(.text)
        *(.rodata*)
    }

    .data : AT(_kernel_LMA_start + SIZEOF(.bootstrap) + SIZEOF(.text))
    {
        *(.data)
    }

    /* Mark the end of the DATA (Stuff we must copy from disk) */
    _kernel_VMA_data_end = .;

    .bss : AT(_kernel_LMA_start + SIZEOF(.bootstrap) + SIZEOF(.text) + SIZEOF(.data))
    {
        _bss_start = .;
        *(.bss)
        *(COMMON)
        _bss_end = .;
    }
    
    _kernel_VMA_end = .;
}

/* --- Calculation Fixes --- */
    
    /* 1. Size to copy from disk (Text + Data only, NO BSS) */
    _binary_size_text_data = SIZEOF(.bootstrap) + SIZEOF(.text) + SIZEOF(.data); ;

    /* 2. Total sectors to load (Stage3 + C Kernel Text/Data) */
    /* We calculate physical file end address minus start address */
    __end_of_file_phys = _kernel_LMA_start + _binary_size_text_data;
    __file_size_bytes = __end_of_file_phys - _start_of_kernel_file;
    __kernel_size_bytes = _kernel_VMA_end - _kernel_VMA_start + SIZEOF(.bootstrap);

    __sector_size = 512;
    __page_size = 1024 * 4;

    __total_sectors = (__file_size_bytes + __sector_size - 1) / __sector_size;
    __total_pages = (__file_size_bytes + __page_size - 1 - __sector_size) / __page_size;