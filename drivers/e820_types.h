#pragma once
#include "includes.h"

typedef struct e820_entry_t {
    uint32_t base_low;
    uint32_t base_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
} e820_entry_t;

typedef struct e820_info_t {
    uint32_t signature;
    uint32_t num_entries;
    e820_entry_t* entries;
    uint64_t address;
} e820_info_t;
