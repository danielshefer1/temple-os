#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "E820.h"
#include "data_structs.h"
#include "print_text.h"

#define PAGE_SIZE 4096
#define KERNEL_VIRTUAL 0x80000000

// External symbols from linker
extern uint32_t __total_pages;
extern uint32_t __total_sectors;
extern uint32_t _kernel_VMA_start;
// Constant (can stay in header since it's computed from extern)
extern uint32_t KERNEL_BASE;

// Page table entry structure
typedef struct pte_t {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t write_thru : 1;
    uint32_t cache_dis  : 1;
    uint32_t accessed   : 1;
    uint32_t dirty      : 1;
    uint32_t pat        : 1;
    uint32_t global     : 1;
    uint32_t avail      : 3;
    uint32_t frame      : 20;
} pte_t;

typedef struct pde_t {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t write_thru : 1;
    uint32_t cache_dis  : 1;
    uint32_t accessed   : 1;
    uint32_t dirty      : 1;
    uint32_t pat        : 1;
    uint32_t global     : 1;
    uint32_t avail      : 3;
    uint32_t frame      : 20;
} pde_t;

// Function declarations
uint32_t num_usable_entries(E820Info* info);
E820Entry* fetch_usable_memory(E820Info* info, E820Entry* usable_entries);
uint32_t page_dir_addr(void);

uint32_t num_page_tables(E820Info* memory_map, const uint32_t kernel_pages);
uint32_t fill_page_table(pte_t* page_table, const E820Entry* usable_memory,
     const uint32_t kernel_pages, const uint16_t idx);