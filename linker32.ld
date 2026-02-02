ENTRY(stage4_entry)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    . = 0x8E00;
    .stage4 { *(.stage4) }

    . = ALIGN(4);
    _bootstrap_LMA_start = .;
    . = 0x100000 /* Bootstrap code is at 1MB */
    _bootstrap_VMA_start = .;

    .text : AT(_bootstrap_LMA_start) { 
        *(.text)
        *(.rodata*)
    }
    _text_end = .;
    . = ALIGN(4);
    _data_start .;

    _data_phy_start = _bootstrap_LMA_start + SIZEOF(.text) + _data_start - _text_end;
    .data : AT(_data_phy_start) { *(.data) }

    _bss_start = _data_phy_start + SIZEOF(.data);
    .bss : AT(_bss_start) {
        *(.bss)
        *(COMMON)
    }
    _bootstrap_VMA_end = .;
}

_bootstrap_size = _bss_start + SIZEOF(.bss) - _bootstrap_LMA_start;

_sector_size = 512;
_page_size = 4096;

__bootstrap_sectors = (_bootstrap_size + _sector_size - 1) / _sector_size;
__bootstrap_pages = (_bootstrap_size + _page_size - 1) / _page_size;