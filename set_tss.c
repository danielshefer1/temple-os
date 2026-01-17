#include "set_tss.h"

static tss_entry_t tss;

void WriteTSS(uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(tss) - 1;

    memset(&tss, 0, sizeof(tss));
    tss.ss0 = ss0;     
    tss.esp0 = esp0;   
    tss.iomap_base = sizeof(tss); 

    SetGDTEntry(base, limit, PRESENT, PRIVILEGE_KERNEL, DESCRIPTOR_TYPE_SYSTEM, 
                0x1, 0, 0, 1, RESERVED, LONG_MODE_32BIT, DEFAULT_BIG_16BIT, GRANULARITY_BYTE, 5);
}

void SetFirstTSS() {
    uint32_t esp0 = AddStack();
    WriteTSS(0x10, esp0);
    load_tss();
}