#pragma once
#include "includes.h"

typedef struct {
    uint8_t  attributes;
    uint8_t  chs_start[3];
    uint8_t  partition_type;
    uint8_t  chs_end[3];
    uint32_t lba_start;
    uint32_t sector_count;
} mbr_partition_t;

typedef struct {
    uint8_t         bootstrap[446]; // Bootloader code area
    mbr_partition_t partitions[4];  // Four partition entries
    uint16_t        signature;      // 0xAA55
} __attribute__((packed)) mbr_t;
