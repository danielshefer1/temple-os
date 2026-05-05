#pragma once
#include "includes.h"

typedef struct interrupt_frame_t {
    // Pushed by isr_common_stub
    uint64_t gs, fs;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8; // PUSHAQ additional regs
    uint64_t rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax;  // PUSHAQ normal regs
    uint64_t int_no, err_code;
    // Pushed by CPU
    uint64_t rip, cs, qflags, userrsp, ss;
} __attribute__((packed)) interrupt_frame_t;

typedef struct idt_entry_t {
    uint16_t base_low;
    uint16_t sel;
    uint8_t ist;
    // ---- Flags ----
    uint8_t gate_type : 4;
    uint8_t storage_segment : 1;
    uint8_t privilege : 2;
    uint8_t present : 1;
    // ----------------
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr_t {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;
