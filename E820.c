#include "E820.h"

e820_info_t* init_E820(uintptr_t address) {
    e820_info_t* info = (e820_info_t*)address;
    info->signature = *(uint32_t*)address;
    if (info->signature != E820_SIGNATURE) {
        kprintf("Invalid E820 Signature: 0x%x\n", info->signature);
        return NULL;
    }
    info->address = (uint32_t) info->entries;
    info->address += KERNEL_VIRTUAL;
    info->entries = get_E820_entries(info->num_entries, info->address);
    return info;
}

e820_entry_t* get_E820_entries(uint32_t count, uint32_t address) {
    e820_entry_t* entries = (e820_entry_t*) address;
    for (size_t i = 0; i < count; ++i) {
        entries[i].base_low = *(uint32_t*)(address + i * sizeof(e820_entry_t));
        entries[i].base_high = *(uint32_t*)(address + i * sizeof(e820_entry_t) + 4);
        entries[i].length_low = *(uint32_t*)(address + i * sizeof(e820_entry_t) + 8);
        entries[i].length_high = *(uint32_t*)(address + i * sizeof(e820_entry_t) + 12);
        entries[i].type = *(uint32_t*)(address + i * sizeof(e820_entry_t) + 16);
    }
    return entries;
}

uint32_t num_valid_entries(e820_info_t* info) {
    uint32_t count = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (info->entries[i].base_high != 0 || info->entries[i].length_high != 0)
            continue;
        count++;
    }
    return count;
}

uint32_t num_usable_entries(e820_info_t* info) {
    uint32_t count = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (isUsableEntry(&info->entries[i])) 
            count++;
    }
    return count;
}

bool isUsableEntry(const e820_entry_t* entry) {
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

void fetch_usable_memory(e820_info_t* info, e820_entry_t* usable_entries) {
    uint32_t index = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (info->entries[i].base_high != 0 || info->entries[i].length_high != 0)
            continue;
            
        if (isUsableEntry(&info->entries[i])) 
            usable_entries[index++] = info->entries[i];
    }
}

void fetch_unusable_memory(e820_info_t* info, e820_entry_t* unusable_entries) {
    uint32_t index = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (info->entries[i].base_high != 0 || info->entries[i].length_high != 0)
            continue;

        if (!isUsableEntry(&info->entries[i])) 
            unusable_entries[index++] = info->entries[i];
    }
}

void print_E820_entrys(e820_entry_t* entries, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        print_E820_entry(entries[i], i);
    }
}

void print_E820_entry(e820_entry_t entry, uint32_t idx) {
    kprintf("Entry %d:\nBase: %8x\tLength: %8x\tType: %d\n",
         idx, entry.base_low, entry.length_low, entry.type);
}