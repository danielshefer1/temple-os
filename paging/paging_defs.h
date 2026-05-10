#pragma once
#include "includes.h"

// Paging Definitions
#define PAGE_SIZE 4096
#define MB 0x100000
#define GB 0x40000000ULL
#define KERNEL_VIRTUAL 0xFFFFFFFF80000000ULL
#define KERNEL_BASE 0x200000
#define USER_VIRTUAL 0x40000000

// User anonymous-mmap region: bump-allocated by MmapHandler, in canonical
// low-half above the PIE/heap range and well below USER_STACK_TOP_VA.
#define USER_MMAP_BASE 0x0000500000000000ULL
#define USER_MMAP_END  0x0000700000000000ULL
#define TABLE_SIZE (PAGE_SIZE * 512)
#define PAGE_SIZE_LOG2 12
#define STACK_PAGES 4
#define KERNEL_VIRT_TO_PHYS(addr) ((uint64_t)(addr) - KERNEL_VIRTUAL)

// Framebuffer aperture. Lives in PDPT slot 509 (fresh — kernel image's PDPT
// slot 510 is fully covered by InitPaging's "headroom" big-page mappings, so
// any 4 KB FB mapping placed in 0xFFFFFFFF80000000..0xFFFFFFFFBFFFFFFF would
// silently hit the EEXIST big-page branch in map_page_to_virt and never get
// installed). Plenty of room here for FB + back-buffer + scrollback later.
#define FB_VIRTUAL 0xFFFFFFFF40000000

#define MMIO_VIRTUAL 0xFFFFFFFFC0000000
#define MMIO_BASE 0x7FFDF000
#define MMIO_OFFSET (MMIO_VIRTUAL - MMIO_BASE)

#define PCI_SCAN_VIRTUAL 0xFFFFFFFFD0000000
#define PCI_SCAN_BASE 0xB0000000
#define PCI_OFFSET (PCI_SCAN_VIRTUAL - PCI_SCAN_BASE)

#define AHCI_VIRTUAL 0xFFFFFFFFE0000000

#define CORE_VIRTUAL 0xFFFFFFFFF0000000

#define PML4_IDX(addr) (((uint64_t)(addr) >> 39) & 0x1FF)
#define PDPT_IDX(addr) (((uint64_t)(addr) >> 30) & 0x1FF)
#define PD_IDX(addr)   (((uint64_t)(addr) >> 21) & 0x1FF)
#define PT_IDX(addr)   (((uint64_t)(addr) >> 12) & 0x1FF)

// Paging Flags Definitions
#define PRESENT_PAGE 0x1
#define PRESENT_PAGE_BIT 0

#define RW_PAGE 0x2
#define RW_PAGE_BIT 1

#define USER_PAGE 0x4
#define USER_PAGE_BIT 2

#define WRITE_THROUGH_PAGE 0x8
#define WRITE_THROUGH_PAGE_BIT 3

#define CACHE_DIS_PAGE 0x10
#define CACHE_DIS_PAGE_BIT 4

#define GLOBAL_PAGE 0x100
#define GLOBAL_PAGE_BIT 8

#define NX_PAGE 0x8000000000000000
#define NX_PAGE_BIT 63

#define BIG_PAGE 0x80
#define BIG_PAGE_BIT 7


#define RW_KERNEL (PRESENT_PAGE | RW_PAGE | GLOBAL_PAGE | NX_PAGE)
#define R_KERNEL (PRESENT_PAGE | GLOBAL_PAGE)

#define RW_USER (PRESENT_PAGE | RW_PAGE | USER_PAGE | NX_PAGE)
#define R_USER (PRESENT_PAGE | USER_PAGE)

#define RW_MMIO (PRESENT_PAGE | RW_PAGE | WRITE_THROUGH_PAGE | CACHE_DIS_PAGE | GLOBAL_PAGE | NX_PAGE)
#define R_MMIO (PRESENT_PAGE | WRITE_THROUGH_PAGE | CACHE_DIS_PAGE | GLOBAL_PAGE)

// Framebuffer mapping. PWT alone (without PCD) selects PAT slot 1, which
// pat_init reprograms to WC (Write-Combining). Pixel stores then coalesce
// into 64-byte burst transactions instead of the per-4-byte serialized
// accesses you get with PA3=UC. Used only by fb_map; MMIO regs stay UC.
#define RW_FB (PRESENT_PAGE | RW_PAGE | WRITE_THROUGH_PAGE | GLOBAL_PAGE | NX_PAGE)
