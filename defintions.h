#pragma once
#include "includes.h"

// Slab Definitions
#define NUM_CACHE 2
#define SLAB_GARBAGE_BYTE 0xAC
// End Slab Definitions

// Buddy Definitions
#define MAX_ORDER 32
// End Buddy Definitions


// Paging Definitions 
#define PAGE_SIZE 4096
#define KERNEL_VIRTUAL 0x80000000
#define KERNEL_BASE 0x100000
#define TABLE_SIZE PAGE_SIZE * 1024
#define HIGHER_HALF_IDX 512
#define PAGE_SIZE_LOG2 12
#define STACK_PAGES 8
// End Paging Definitions 

// E820 Definitions
#define E820_SIGNATURE 0x534D4150
#define E820_ADDRESS KERNEL_VIRTUAL + 0x500
// End E820 Definitions

// VGA Definitions
#define VGA_BUFFER ((volatile char*)(uint32_t) KERNEL_VIRTUAL + 0xB8000)
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

// Timer Definitions
#define TIMER_FREQUENCY 1000
#define ONE_SEC 1000
#define HALF_SEC 500
// End Timer Definitions

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
