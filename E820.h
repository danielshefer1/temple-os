#pragma once
#include <stdint.h>
#include <stddef.h>
#include "print_text.h"

#define E820_SIGNATURE 0x534D4150
#define E820_ADDRESS 0x500

typedef struct E820Entry {
    uint32_t base_low;    // Lower 32 bits of base
    uint32_t base_high;   // Upper 32 bits of base  
    uint32_t length_low;  // Lower 32 bits of length
    uint32_t length_high; // Upper 32 bits of length
    uint32_t type;
} E820Entry;

typedef struct E820Info {
    uint32_t signature;
    uint32_t num_entries;
    E820Entry* entries;
    uint32_t address;
} E820Info;

E820Info* init_E820(uintptr_t address);
E820Entry* get_E820_entries(uint32_t count, uint32_t address);
void print_memory_map(E820Info* info);
void print_memory_entry(E820Entry* entry, uint32_t i);