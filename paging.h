#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "E820.h"
#include "data_structs.h"
#include "print_text.h"

#define PAGE_SIZE 4096
#define KERNEL_VIRTUAL 0x80000000
#define KERNEL_BASE 0x100000
#define TABLE_SIZE PAGE_SIZE * 1024
#define HIGHER_HALF_IDX 512

// External symbols from linker
extern uint32_t __total_pages;
extern uint32_t __total_sectors;
extern uint32_t _kernel_VMA_start;

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
uint32_t page_dir_addr(void);

void InitPageDirectory(pde_t* page_directory, uint32_t pd_addr, uint32_t kernel_pages);
void InitPageTable(pte_t* page_table, uint32_t kernel_pages);