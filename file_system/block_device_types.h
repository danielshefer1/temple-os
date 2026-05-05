#pragma once
#include "includes.h"

typedef struct block_device {
    char name[32];
    uint64_t total_sectors;
    uint32_t sector_size;
    void* device_specific_ptr;

    int64_t (*read)(struct block_device* dev, uint64_t lba, uint32_t count, void* buffer);
    int64_t (*write)(struct block_device* dev, uint64_t lba, uint32_t count, void* buffer);
    int64_t (*flush)(struct block_device* dev);
} block_device_t;

typedef struct partition_device {
    block_device_t* physical_device;
    uint64_t start_lba;
    uint64_t sector_count;
    uint8_t partition_type;

} partition_device_t;

typedef struct block_device_node {
    block_device_t* value;
    struct block_device_node* next;
} block_device_node_t;

typedef struct partition_device_node {
    partition_device_t* value;
    struct partition_device_node* next;
} partition_device_node_t;
