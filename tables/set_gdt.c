#include "set_gdt.h"
#include "cpu_local.h"

// 5 fixed entries (null, kernel CS, kernel DS, user DS, user CS) + 2 slots per CPU
// for 16-byte 64-bit TSS descriptors.
static gdt_entry_t gdt[5 + 2 * MAX_CPUS];
static gdt_ptr_t gdtr;

void SetGDTEntry(uint64_t base, uint64_t limit, uint8_t present, uint8_t privilege, uint8_t type,
     uint8_t exec, uint8_t dir_conf, uint8_t wr, uint8_t access, uint8_t reserved,
     uint8_t long_mode, uint8_t default_big, uint8_t granularity, uint64_t idx) {
    gdt_entry_t* entry = &gdt[idx];

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
    gdt_ptr_t current_gdtr;
    
    __asm__ volatile("sgdt %0" : "=m"(current_gdtr));
    
    uint64_t virtual_base = current_gdtr.base + KERNEL_VIRTUAL;
    
    kprintf("GDTR Base (Physical): 0x%x\n", current_gdtr.base);
    kprintf("GDTR Base (Virtual): 0x%x\n", virtual_base);
    kprintf("GDTR Limit: 0x%x\n", current_gdtr.limit);
    kprintf("Expected Base (Physical): 0x%x\n", (uint64_t)&gdt - KERNEL_VIRTUAL);
    kprintf("Expected Limit: 0x%x\n", sizeof(gdt) - 1);
}

void SetGDT() {
    // Null segment
    SetGDTEntry(0, 0, 0, 0, 0, 
                0, 0, 0, 
                0, 0, 0, 0, 0, 0);
    
    // Kernel code segment
    SetGDTEntry(0, 0, PRESENT, PRIVILEGE_KERNEL, DESCRIPTOR_TYPE_CODE_DATA, 
                TYPE_CODE_EXECUTABLE, TYPE_CODE_CONFORMING, TYPE_DATA_READABLE_WRITABLE, 
                TYPE_ACCESSSED, RESERVED, LONG_MODE_64BIT, DEFAULT_BIG_16BIT, GRANULARITY_4KB, 1);
    
    // Kernel data segment
    SetGDTEntry(0, 0, PRESENT, PRIVILEGE_KERNEL, DESCRIPTOR_TYPE_CODE_DATA, 
                TYPE_DATA_NON_EXECUTABLE, TYPE_DATA_EXPAND_DOWN, TYPE_DATA_READABLE_WRITABLE, 
                TYPE_ACCESSSED, RESERVED, LONG_MODE_32BIT, DEFAULT_BIG_32BIT, GRANULARITY_4KB, 2);
    
    // User mode data segment (slot 3) — must precede user code so SYSRET selectors line up:
    // STAR[63:48] = 0x10 → SYSRET sets SS = 0x10+8 = 0x18 (this entry), CS = 0x10+16 = 0x20 (next).
    SetGDTEntry(0, 0, PRESENT, PRIVILEGE_USER, DESCRIPTOR_TYPE_CODE_DATA,
                TYPE_DATA_NON_EXECUTABLE, TYPE_DATA_EXPAND_DOWN, TYPE_DATA_READABLE_WRITABLE,
                TYPE_ACCESSSED, RESERVED, LONG_MODE_32BIT, DEFAULT_BIG_32BIT, GRANULARITY_4KB, 3);

    // User mode code segment (slot 4)
    SetGDTEntry(0, 0, PRESENT, PRIVILEGE_USER, DESCRIPTOR_TYPE_CODE_DATA,
                TYPE_CODE_EXECUTABLE, TYPE_CODE_CONFORMING, TYPE_DATA_READABLE_WRITABLE,
                TYPE_ACCESSSED, RESERVED, LONG_MODE_64BIT, DEFAULT_BIG_16BIT, GRANULARITY_4KB, 4);
    
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)gdt;

    LoadGDTHelper(&gdtr);

    //CheckGDT();
}

gdt_ptr_t* getGdtPointer() {
    return &gdtr;
}

// 64-bit TSS descriptors are 16 bytes — they occupy two consecutive 8-byte
// GDT slots. slot_idx is the index of the first slot.
void SetTSSDescriptor(uint64_t base, uint32_t limit, uint64_t slot_idx) {
    uint64_t* slot = (uint64_t*)&gdt[slot_idx];

    uint64_t low = 0;
    low |= (uint64_t)(limit & 0xFFFF);
    low |= ((uint64_t)(base & 0xFFFFFF)) << 16;
    low |= ((uint64_t)0x89) << 40;                       // P=1, DPL=0, type=0x9 (avail 64-bit TSS)
    low |= ((uint64_t)((limit >> 16) & 0xF)) << 48;      // limit[19:16]; flags G/L/D/AVL all 0
    low |= ((uint64_t)((base >> 24) & 0xFF)) << 56;
    slot[0] = low;

    slot[1] = (base >> 32) & 0xFFFFFFFFull;
}