#pragma once
#include "includes.h"

typedef struct gdt_entry_t {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    // ---- Access byte ----
    uint8_t  accessed : 1;
    uint8_t  readable_writable : 1;
    uint8_t  direction_conforming : 1;
    uint8_t  executable : 1;
    uint8_t  descriptor_type : 1;
    uint8_t  privilege : 2;
    uint8_t  present : 1;
    // --------------------
    uint8_t  limit_high : 4;
    // ---- Flags ----
    uint8_t reserved : 1;
    uint8_t  long_mode : 1;
    uint8_t  default_big : 1;
    uint8_t  granularity : 1;
    // ----------------
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct gdt_ptr_t {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct tss64_t {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss64_t;
