#include "set_tss.h"

static tss_entry_t tss;

void WriteTSS(uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(tss) - 1;

    // Initialize TSS structure
    memset(&tss, 0, sizeof(tss));
    tss.ss0 = ss0;     // Kernel Data Segment (usually 0x10)
    tss.esp0 = esp0;   // The top of your kernel stack
    tss.iomap_base = sizeof(tss); // Point it beyond the limit to disable IO map

    // Add TSS to GDT: 
    // Type 0x9 (1001 binary) = Available 32-bit TSS
    // Privilege 0 or 3 (Usually 0, but some use 3 if they want to allow task switching via JMP)
    SetGDTEntry(base, limit, PRESENT, PRIVILEGE_KERNEL, DESCRIPTOR_TYPE_SYSTEM, 
                0x1, 0, 0, 1, RESERVED, LONG_MODE_32BIT, DEFAULT_BIG_16BIT, GRANULARITY_BYTE, 5);
}

void SetFirstTSS() {
    uint32_t esp0 = AddStack();
    WriteTSS(0x10, esp0);
    load_tss();
}