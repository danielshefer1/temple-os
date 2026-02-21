#include "mbr.h"

void ParseMbr(uint8_t* buffer) {
    mbr_t* mbr = (mbr_t*)buffer;

    // 1. Check Signature first
    if (mbr->signature != 0xAA55) {
        kprintf("Error: MBR Signature mismatch (Expected 0xAA55, got 0x%x)\n", mbr->signature);
        return;
    }

    kprintf("--- MBR Partition Table ---\n");

    for (int i = 0; i < 4; i++) {
        mbr_partition_t* part = &mbr->partitions[i];

        if (part->sector_count == 0) continue;

        kprintf("Partition #%d:\n", i);
        kprintf("  Bootable: %s\n", (part->attributes & 0x80) ? "Yes" : "No");
        kprintf("  Type: %x\n", part->partition_type);
        kprintf("  Start LBA: %d\n", part->lba_start);
        kprintf("  Sectors: %d\n", part->sector_count);
        
        uint64_t size_mb = ((uint64_t)part->sector_count * 512) / (1024 * 1024);
        kprintf("  Size: %d MB\n", (uint32_t)size_mb);
    }
}