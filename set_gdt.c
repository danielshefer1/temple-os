#include "set_gdt.h"

static gdt_entry gdt[5];
static gdt_ptr gdtr;

void SetGDTEntry(uint32_t base, uint32_t limit, uint8_t present, uint8_t privilege, uint8_t type,
     uint8_t exec, uint8_t dir_conf, uint8_t wr, uint8_t access, uint8_t reserved,
     uint8_t long_mode, uint8_t default_big, uint8_t granularity, uint32_t idx) {
    gdt_entry* entry = &gdt[idx];

    entry->limit_low = (limit & 0xFFFF);
    entry->base_low = (base & 0xFFFF);
    entry->base_middle = (base >> 16) & 0xFF;

    entry->accessed = access;
    entry->readable_writable = wr;
    entry->direction_conforming = dir_conf;
    entry->executable = exec;
    entry->descriptor_type = type;
    entry->privilege = privilege;
    entry->present = present;


    entry->reserved = reserved;
    entry->long_mode = long_mode;
    entry->default_big = default_big;
    entry->granularity = granularity;
    
    entry->limit_high = (limit >> 16) & 0x0F;
    entry->base_high = (base >> 24) & 0xFF;
}

void LoadGDT(gdt_entry* gdt_ptr, uint16_t size) {
    gdtr.limit = size - 1;
    gdtr.base = (uint32_t)gdt_ptr;
    LoadGDTHelper(&gdtr);
}

void SetGDT() {
    // Null segment
    SetGDTEntry(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    // Code segment
    SetGDTEntry(0, 0xFFFFF, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1);
    // Data segment
    SetGDTEntry(0, 0xFFFFF, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 2);
    // User mode code segment
    SetGDTEntry(0, 0xFFFFF, 1, 3, 1, 1, 0, 1, 0, 0, 0, 1, 1, 3);
    // User mode data segment
    SetGDTEntry(0, 0xFFFFF, 1, 3, 1, 0, 0, 1, 0, 0, 0, 1, 1, 4);

    LoadGDT(gdt, sizeof(gdt));
}