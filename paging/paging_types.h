#pragma once
#include "includes.h"
#include "lock_types.h"

typedef struct {
    uint64_t present    : 1;
    uint64_t writable   : 1;
    uint64_t user       : 1;
    uint64_t pwt        : 1;
    uint64_t pcd        : 1;
    uint64_t accessed   : 1;
    uint64_t dirty      : 1;
    uint64_t page_size  : 1;
    uint64_t global     : 1;
    uint64_t available  : 3;
    uint64_t address    : 40;
    uint64_t reserved   : 11;
    uint64_t no_execute : 1;
} __attribute__((packed)) page_entry_t;

typedef struct tlb_shootdown_t {
    spinlock_t lock;             // serializes initiators
    volatile uint64_t addr;      // 0 = full flush, otherwise single page
    volatile uint64_t pending;   // bitmap of CPUs we are still waiting on
} tlb_shootdown_t;
