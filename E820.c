#include "E820.h"

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

void print_memory_entry(E820Entry* entry, uint32_t i) {
    if (entry->base_high == 0 && entry->length_high == 0) {
            // Print as single 32-bit values
            kprintf("Entry %d: Base=0x%x, Length=0x%x, Type=%d\n",
                    i, 
                    entry->base_low,
                    entry->length_low,
                    entry->type);
            return;
        }
        else if (entry->base_high == 0) {
            // Print base as 32-bit and length as 64-bit
            kprintf("Entry %d: Base=0x%x, Length=0x%x%8x, Type=%d\n",
                    i, 
                    entry->base_low,
                    entry->length_high, entry->length_low,
                    entry->type);
            return;
        }
        else if (entry->length_high == 0) {
            // Print base as 64-bit and length as 32-bit
            kprintf("Entry %d: Base=0x%x%8x, Length=0x%x, Type=%d\n",
                    i, 
                    entry->base_high, entry->base_low,
                    entry->length_low,
                    entry->type);
            return;
        }
        
        // Print as two 32-bit values instead
        kprintf("Entry %d: Base=0x%x%8x, Length=0x%x%8x, Type=%d\n",
                i, 
                entry->base_high, entry->base_low,
                entry->length_high, entry->length_low,
                entry->type);
}


void print_memory_map(E820Info* info) {
    kprintf("E820 Memory Map:\n");
    kprintf("Signature: 0x%x\n", info->signature);
    kprintf("Number of Entries: %d\n", info->num_entries);
    
    for (size_t i = 0; i < info->num_entries; ++i) {
        E820Entry* entry = &info->entries[i];
        print_memory_entry(entry, i);
    }
}

uint32_t num_usable_entries(E820Info* info) {
    uint32_t count = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (info->entries[i].type != 1) {
            continue;
        }
        if (info->entries[i].length_low < PAGE_SIZE && info->entries[i].length_high != 0) {
            continue;
        }
        if (info->entries[i].base_high != 0 || info->entries[i].base_low == 0) {
            continue;
        }
        count++;
    }
    return count;
}

bool isUsableEntry(const E820Entry* entry) {
    if (entry->type != 1) {
        return false;
    }
    if (entry->length_low < PAGE_SIZE && entry->length_high != 0) {
        return false;
    }
    if (entry->base_high != 0 || entry->base_low == 0) {
        return false;
    }
    return true;
}

void fetch_usable_memory(E820Info* info, E820Entry* usable_entries) {
    uint32_t index = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (isUsableEntry(&info->entries[i])) 
            usable_entries[index++] = info->entries[i];
    }
}

void fetch_unusable_memory(E820Info* info, E820Entry* unusable_entries) {
    uint32_t index = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (!isUsableEntry(&info->entries[i])) 
            unusable_entries[index++] = info->entries[i];
    }
}