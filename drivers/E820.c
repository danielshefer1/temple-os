#include "E820.h"

e820_info_t* init_E820(uintptr_t address) {
    e820_info_t* info = (e820_info_t*)address;
    info->signature = *(uint64_t*)address;
    if (info->signature != E820_SIGNATURE) {
        kprintf("Invalid E820 Signature: 0x%x\n", info->signature);
        return NULL;
    }
    info->address = (uint64_t) info->entries;
    info->address += KERNEL_VIRTUAL;
    info->entries = (e820_entry_t*) info->address;
    return info;
}