#pragma once
#include "includes.h"

// Slab Definitions
#define SLAB_GARBAGE_BYTE 0xAC
// End Slab Definitions

// Buddy Definitions
#define MAX_ORDER 64
// End Buddy Definitions


// Paging Definitions 
#define PAGE_SIZE 4096
#define MB 0x100000
#define GB 0x40000000ULL
#define KERNEL_VIRTUAL 0xFFFFFFFF80000000ULL
#define KERNEL_BASE 0x200000
#define USER_VIRTUAL 0x40000000
#define TABLE_SIZE (PAGE_SIZE * 512)
#define PAGE_SIZE_LOG2 12
#define STACK_PAGES 4
#define KERNEL_VIRT_TO_PHYS(addr) ((uint64_t)(addr) - KERNEL_VIRTUAL)

#define MMIO_VIRTUAL 0xFFFFFFFFC0000000
#define MMIO_BASE 0x7FFDF000
#define MMIO_OFFSET (MMIO_VIRTUAL - MMIO_BASE)

#define PCI_SCAN_VIRTUAL 0xFFFFFFFFD0000000 
#define PCI_SCAN_BASE 0xB0000000
#define PCI_OFFSET (PCI_SCAN_VIRTUAL - PCI_SCAN_BASE)

#define AHCI_VIRTUAL 0xFFFFFFFFE0000000


#define PML4_IDX(addr) (((uint64_t)(addr) >> 39) & 0x1FF)
#define PDPT_IDX(addr) (((uint64_t)(addr) >> 30) & 0x1FF)
#define PD_IDX(addr)   (((uint64_t)(addr) >> 21) & 0x1FF)
#define PT_IDX(addr)   (((uint64_t)(addr) >> 12) & 0x1FF)
// End Paging Definitions 

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
// End Paging Flags Definitions

// E820 Definitions
#define E820_SIGNATURE 0x534D4150
#define E820_ADDRESS  (KERNEL_VIRTUAL + 0x500)
// End E820 Definitions

// VGA Definitions
#define VGA_BUFFER ((volatile char*)((uint64_t)KERNEL_VIRTUAL + 0xB8000))
#define GREY_COLOR 0x07
#define RED_COLOR 0x04
#define CURSOR_START 14
#define CURSOR_END 15
// End VGA Definitions

// GDT Definitions
#define GDT_LIMIT 0xFFFFF
#define GDT_BASE 0x0
#define PRESENT 1
#define NOT_PRESENT 0
#define TYPE_DATA_NON_EXECUTABLE 0
#define PRIVILEGE_KERNEL 0
#define PRIVILEGE_USER 3
#define DESCRIPTOR_TYPE_CODE_DATA 1
#define DESCRIPTOR_TYPE_SYSTEM 0
#define TYPE_CODE_EXECUTABLE 1
#define TYPE_DATA_READABLE_WRITABLE 1
#define TYPE_DATA_EXPAND_DOWN 0
#define TYPE_CODE_CONFORMING 0
#define TYPE_ACCESSSED 0
#define GRANULARITY_4KB 1
#define GRANULARITY_BYTE 0
#define DEFAULT_BIG_32BIT 1
#define DEFAULT_BIG_16BIT 0
#define LONG_MODE_64BIT 1
#define LONG_MODE_32BIT 0
#define RESERVED 0
// End GDT Definitions

// IDT Definitions
#define IDT_SIZE 256
#define GDT_CODE_SEGMENT 0x08
#define IDT_TYPE_INTERRUPT_GATE 0xE
#define IDT_TYPE_TRAP_GATE 0xF
#define SYS_CALL 0x80
#define TIMER_IDT 32
// End IDT Definitions

// System Calls Definitions
#define EXIT_SYSCALL 1
#define WRITE_SYSCALL 2
#define READ_SYSCALL 3
#define FLUSH_BUFFER_SYSCALL 4
#define MMAP_SYSCALL 5
#define MUNMAP_SYSCALL 6
// System Calls Definitions End

// Keyboard Definitions
#define LEFT_SHIFT_MAKE_SCANCODE 0x2A
#define LEFT_SHIFT_BREAK_SCANCODE 0xAA
#define RIGHT_SHIFT_MAKE_SCANCODE 0x36
#define RIGHT_SHIFT_BREAK_SCANCODE 0xB6
// End Keyboard Definitions

// PIC Definitions
#define PIC_TIMER_FREQUENCY 100
#define MASTER_PIC 0x21
#define SLAVE_PIC 0xA1
// End PIC Definitions

// Timer Definitions
#define TIMER_TICK_PER_MS 1
#define SEC 1000
// End Timer Definitions

// Apic Definitions
#define IOAPIC_REG_INDEX    0x00
#define IOAPIC_REG_DATA     0x10
#define IOAPIC_REDTBL_BASE  0x10 
// End Apic Definitions


// Keyboard Definitions
#define CONSOLE_BUFFER_SIZE 512
#define KEYBOARD_MS_BACK 10
// End Keyboard Definitions

// Serial Definitions
#define COM1_BASE 0x3F8
#define COM1_FCR COM1_BASE + 2
#define COM1_IIR COM1_BASE + 2
#define COM1_LSR COM1_BASE + 5
#define MASTER_PIC_IMR 0x21
#define IRQ_COM1 4
// End Serial Definitions

// VFS Definitons
#define VFS_FILE 0
#define VFS_DIRECTORY 1
#define MOUNT_POINT 2
#define SYS_LINK 3

#define MAX_FILE_NAME_SIZE 256
// End VFS Definitons

// Dcache Definitions
#define GOLDEN_RATIO_32 0x61C88647
#define DCACHE_SIZE 1024
// End Dcache Definitions

// RSDT Definitions
#define RSDP_SIG_LENGTH 8
#define OEM_ID_LENGTH 6
#define ACPI_TABLE_SIG_LEGNTH 4
#define RSDT_HEADER_LENGTH 36
#define MADT_HEADER_LENGTH 44
#define MMIO_PHYS_TO_VIRT(phys) ((uint64_t)phys + MMIO_OFFSET)
// End RSDT Definitions

// MultiCore Definitions
#define TRAMPOLINE_ADDR 0xA000
// End MultiCore Definitions

// PCI Defintions
#define AHCI_CLASS 0x01
#define AHCI_SUBCLASS 0x06
#define xHCI_CLASS 0x0C
#define xHCI_SUBCLASS 0x03

#define MSI_CAP 0x05

// End PCI Defintions

// AHCI Definitions
#define BIOS_TIMEOUT 2000
#define AHCI_INT_VECTOR 0x40
#define CMD_TABLE_SIZE 256
#define RECV_FIS_SIZE 256 
#define CMD_LIST_HEADER_SIZE 32
#define CMD_LIST_PAGES(CMD_LIST_SIZE) (RECV_FIS_SIZE + (CMD_LIST_HEADER_SIZE + CMD_TABLE_SIZE) * CMD_LIST_SIZE + PAGE_SIZE - 1) / PAGE_SIZE
#define H2D_FIS_SIZE 20

#define SATA_SIG_ATA    0x00000101 
#define SATA_SIG_ATAPI  0xEB140101
// End AHCI Definitions