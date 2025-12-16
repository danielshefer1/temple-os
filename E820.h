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

E820Info* init_E820(uintptr_t address) {
    E820Info* info = (E820Info*)address;
    info->signature = *(uint32_t*)address;
    if (info->signature != E820_SIGNATURE) {
        kprintf("Invalid E820 Signature: 0x%x\n", info->signature);
        return NULL;
    }

    info->num_entries = *(uint32_t*)(address + 4);
    info->address = *(uint32_t*)(address + 8);
    info->entries = get_E820_entries(info->num_entries, info->address);
    return info;
}


E820Entry* get_E820_entries(uint32_t count, uint32_t address) {
    E820Entry* entries = (E820Entry*)address;
    for (size_t i = 0; i < count; ++i) {
        entries[i].base_low = *(uint32_t*)(address + i * sizeof(E820Entry));
        entries[i].base_high = *(uint32_t*)(address + i * sizeof(E820Entry) + 4);
        entries[i].length_low = *(uint32_t*)(address + i * sizeof(E820Entry) + 8);
        entries[i].length_high = *(uint32_t*)(address + i * sizeof(E820Entry) + 12);
        entries[i].type = *(uint32_t*)(address + i * sizeof(E820Entry) + 16);
    }
    return entries;
}

void print_memory_map(E820Info* info) {
    kprintf("E820 Memory Map:\n");
    kprintf("Signature: 0x%x\n", info->signature);
    kprintf("Number of Entries: %d\n", info->num_entries);
    
    for (size_t i = 0; i < info->num_entries; ++i) {
        E820Entry* entry = &info->entries[i];

        if (entry->base_high == 0 && entry->length_high == 0) {
            // Print as single 32-bit values
            kprintf("Entry %d: Base=0x%x, Length=0x%x, Type=%d\n",
                    i, 
                    entry->base_low,
                    entry->length_low,
                    entry->type);
            continue;
        }
        else if (entry->base_high == 0) {
            // Print base as 32-bit and length as 64-bit
            kprintf("Entry %d: Base=0x%x, Length=0x%x%8x, Type=%d\n",
                    i, 
                    entry->base_low,
                    entry->length_high, entry->length_low,
                    entry->type);
            continue;
        }
        else if (entry->length_high == 0) {
            // Print base as 64-bit and length as 32-bit
            kprintf("Entry %d: Base=0x%x%8x, Length=0x%x, Type=%d\n",
                    i, 
                    entry->base_high, entry->base_low,
                    entry->length_low,
                    entry->type);
            continue;
        }
        
        // Print as two 32-bit values instead
        kprintf("Entry %d: Base=0x%x%8x, Length=0x%x%8x, Type=%d\n",
                i, 
                entry->base_high, entry->base_low,
                entry->length_high, entry->length_low,
                entry->type);
    }
}