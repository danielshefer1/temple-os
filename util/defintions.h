#pragma once
#include "includes.h"

// Slab Definitions
#define SLAB_GARBAGE_BYTE 0xAC
#define SLAB_WOS_CODE 0xFFFFFFFE
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

#define CORE_VIRTUAL 0xFFFFFFFFF0000000

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
#define SPACE_CHAR 0x20
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
#define VFS_TYPE_FILE      0x01
#define VFS_TYPE_DIR       0x02
#define VFS_TYPE_SYMLINK   0x03
#define VFS_TYPE_CHARDEV   0x04
#define VFS_TYPE_BLOCKDEV  0x05
#define VFS_TYPE_FIFO      0x06
#define VFS_TYPE_SOCKET    0x07
#define VFS_TYPE_UNKNOWN   0x00

#define EOK          0    // success
#define EPERM        1    // operation not permitted (wrong permissions)
#define ENOENT       2    // no such file or directory
#define EIO          5    // I/O error (disk read/write failed)
#define EBADF        9    // bad file descriptor
#define ENOMEM       12   // out of memory
#define EACCES       13   // permission denied
#define EBUSY        16   // device or resource busy
#define EEXIST       17   // file already exists
#define ENOTDIR      20   // not a directory
#define EISDIR       21   // is a directory (tried to read a dir as a file)
#define EINVAL       22   // invalid argument
#define ENOSPC       28   // no space left on device
#define EROFS        30   // read only filesystem
#define ENOTEMPTY    39   // directory not empty (tried to rmdir non-empty dir)
#define ENOTSUP      95   // operation not supported

#define S_SYNC        1   // Writes are synced at once
#define S_IMMUTABLE   2   // Immutable file
#define S_APPEND      4   // Append-only file
#define S_NOATIME     8   // Do not update access times
#define S_NODUMP     16   // Do not dump

#define IS_SYNC(inode)      ((inode)->flags & S_SYNC)
#define IS_IMMUTABLE(inode) ((inode)->flags & S_IMMUTABLE)
#define IS_APPEND(inode)    ((inode)->flags & S_APPEND)
#define IS_NOATIME(inode)   ((inode)->flags & S_NOATIME)
#define IS_NODUMP(inode)    ((inode)->flags & S_NODUMP)
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

#define MAX_PORTS 32
#define SECTOR_SIZE 512
// End AHCI Definitions

// FAT32 Definitions
#define ROOT_LABEL "TEMPLE_OS_ROOT"
#define ROOT_LABEL_LENGTH 9

#define FAT32_BAD     0x0FFFFFF7  
#define FAT32_FREE    0x00000000 

#define MAX_FILENAME_FAT32 256

#define END_ENTRY 0x00
#define DELETED_ENTRY 0xE5


#define LFN_ATTR 0xF
#define OS_ATTR 0x4
#define VOLUME_LABEL_ATTR 0x8

#define LAST_LFN 0x40

#define FAT32_MAGIC 0xFA732000
// End FAT32 Definitions

// EXT2 Definitions

#define EXT2_S_IFSOCK  0xC000
#define EXT2_S_IFLNK   0xA000
#define EXT2_S_IFREG   0x8000
#define EXT2_S_IFBLK   0x6000
#define EXT2_S_IFDIR   0x4000
#define EXT2_S_IFCHR   0x2000
#define EXT2_S_IFIFO   0x1000

#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7

// i_mode permission bits
#define EXT2_S_ISUID   0x0800       // setuid
#define EXT2_S_ISGID   0x0400       // setgid
#define EXT2_S_ISVTX   0x0200       // sticky
#define EXT2_S_IRUSR   0x0100
#define EXT2_S_IWUSR   0x0080
#define EXT2_S_IXUSR   0x0040
#define EXT2_S_IRGRP   0x0020
#define EXT2_S_IWGRP   0x0010
#define EXT2_S_IXGRP   0x0008
#define EXT2_S_IROTH   0x0004
#define EXT2_S_IWOTH   0x0002
#define EXT2_S_IXOTH   0x0001

// i_flags
#define EXT2_SECRM_FL        0x00000001   // secure deletion
#define EXT2_UNRM_FL         0x00000002   // record for undelete
#define EXT2_COMPR_FL        0x00000004   // compressed file
#define EXT2_SYNC_FL         0x00000008   // synchronous updates
#define EXT2_IMMUTABLE_FL    0x00000010   // immutable file
#define EXT2_APPEND_FL       0x00000020   // append only
#define EXT2_NODUMP_FL       0x00000040   // do not dump
#define EXT2_NOATIME_FL      0x00000080   // do not update atime

#define EXT2_MAGIC              0xEF53
#define EXT2_SUPERBLOCK_OFFSET  1024   // superblock always at byte 1024
#define EXT2_SUPERBLOCK_LENGTH 1024

#define EXT2_VALID_FS    0x0001   // cleanly unmounted
#define EXT2_ERROR_FS    0x0002   // not cleanly unmounted / has errors
#define EXT2_ORPHAN_FS   0x0004   // orphan inodes being recovered

#define EXT2_SYMLINK_PREM 0777
#define MAX_FAST_SYMLINK_LENGTH 60

// inode numbers reserved by ext2 (1-10)
#define EXT2_BAD_INO            1      // bad blocks inode
#define EXT2_ROOT_INO           2      // root directory
#define EXT2_ACL_IDX_INO        3
#define EXT2_ACL_DATA_INO       4
#define EXT2_BOOT_LOADER_INO    5
#define EXT2_UNDEL_DIR_INO      6
#define EXT2_FIRST_INO          11     // first non-reserved inode

#define EXT2_BLOCK_SIZE(sb)         (1024 << (sb)->s_log_block_size)
#define EXT2_BLOCKS_PER_BLOCK(sb)     (sb->block_size / sizeof(uint32_t))
#define EXT2_DIRENT_ALIGN(size) (((size) + 3) & ~3)
// End EXT2 Definitions

// RTC Definitions
#define RTC_OUT_PORT 0x70
#define RTC_IN_PORT 0x71

#define RTC_DAY_REG 0x07
#define RTC_MONTH_REG 0x08
#define RTC_YEAR_REG 0x09

#define RTC_SEC_REG   0x00
#define RTC_MIN_REG   0x02
#define RTC_HOUR_REG  0x04

#define RTC_HANG_REG 0x0A
#define RTC_STATUS_REG 0x0B
// End RTC Definitions

// Buffer Cache Definitions
#define BUFFER_CACHE_CAP 4096
#define BUFFER_CACHE_SIZE 512
// End Buffer Cache Definitions

// Mutex Definitions
#define MUTEX_INITIALIZER {0, 0, NULL}
// End Mutex Definitions