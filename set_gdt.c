#include "set_gdt.h"

static gdt_entry gdt[6];
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

void CheckGDT() {
    gdt_ptr current_gdtr;
    
    __asm__ volatile("sgdt %0" : "=m"(current_gdtr));
    
    uint32_t virtual_base = current_gdtr.base + KERNEL_VIRTUAL;
    
    kprintf("GDTR Base (Physical): 0x%x\n", current_gdtr.base);
    kprintf("GDTR Base (Virtual): 0x%x\n", virtual_base);
    kprintf("GDTR Limit: 0x%x\n", current_gdtr.limit);
    kprintf("Expected Base (Physical): 0x%x\n", (uint32_t)&gdt - KERNEL_VIRTUAL);
    kprintf("Expected Limit: 0x%x\n", sizeof(gdt) - 1);
}

void SetGDT() {
    // Null segment
    // Null segment
    SetGDTEntry(GDT_BASE, 0, NOT_PRESENT, PRIVILEGE_KERNEL, DESCRIPTOR_TYPE_SYSTEM, 
                TYPE_DATA_NON_EXECUTABLE, TYPE_DATA_EXPAND_DOWN, TYPE_DATA_READABLE_WRITABLE, 
                TYPE_ACCESSSED, RESERVED, LONG_MODE_32BIT, DEFAULT_BIG_16BIT, GRANULARITY_BYTE, 0);
    
    // Kernel code segment
    SetGDTEntry(GDT_BASE, GDT_LIMIT, PRESENT, PRIVILEGE_KERNEL, DESCRIPTOR_TYPE_CODE_DATA, 
                TYPE_CODE_EXECUTABLE, TYPE_CODE_CONFORMING, TYPE_DATA_READABLE_WRITABLE, 
                TYPE_ACCESSSED, RESERVED, LONG_MODE_32BIT, DEFAULT_BIG_32BIT, GRANULARITY_4KB, 1);
    
    // Kernel data segment
    SetGDTEntry(GDT_BASE, GDT_LIMIT, PRESENT, PRIVILEGE_KERNEL, DESCRIPTOR_TYPE_CODE_DATA, 
                TYPE_DATA_NON_EXECUTABLE, TYPE_DATA_EXPAND_DOWN, TYPE_DATA_READABLE_WRITABLE, 
                TYPE_ACCESSSED, RESERVED, LONG_MODE_32BIT, DEFAULT_BIG_32BIT, GRANULARITY_4KB, 2);
    
    // User mode code segment
    SetGDTEntry(GDT_BASE, GDT_LIMIT, PRESENT, PRIVILEGE_USER, DESCRIPTOR_TYPE_CODE_DATA, 
                TYPE_CODE_EXECUTABLE, TYPE_CODE_CONFORMING, TYPE_DATA_READABLE_WRITABLE, 
                TYPE_ACCESSSED, RESERVED, LONG_MODE_32BIT, DEFAULT_BIG_32BIT, GRANULARITY_4KB, 3);
    
    // User mode data segment
    SetGDTEntry(GDT_BASE, GDT_LIMIT, PRESENT, PRIVILEGE_USER, DESCRIPTOR_TYPE_CODE_DATA, 
                TYPE_DATA_NON_EXECUTABLE, TYPE_DATA_EXPAND_DOWN, TYPE_DATA_READABLE_WRITABLE, 
                TYPE_ACCESSSED, RESERVED, LONG_MODE_32BIT, DEFAULT_BIG_32BIT, GRANULARITY_4KB, 4);
    
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint32_t)gdt;

    LoadGDTHelper(&gdtr);

    //CheckGDT();
}