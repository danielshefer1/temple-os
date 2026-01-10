#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"

// Function declarations
uint32_t page_dir_addr(void);

void InitPageDirectory(pde_t* page_directory, uint32_t pd_addr, uint32_t kernel_pages, uint32_t text_size);
void InitPageTable(pte_t* page_table, uint32_t kernel_pages, uint32_t text_size, bool is_global);