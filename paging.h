#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "slab_alloc.h"

void AddGuardPage(uint32_t Tidx, uint32_t Pidx);
uint32_t PageDirAddrV();
void InitPaging();
uint32_t AddKernelPages(uint32_t num_pages);
uint32_t AddUserPageTable(uint32_t table_idx);
void FillUserPageTable(uint32_t table_idx, uint32_t start_page, uint32_t num_pages);
void RemovePageTables(uint32_t start_table, uint32_t end_table);
void FillPageDirectoryUser(void* addr, uint32_t size);
void FillPageDirectoryMMIO(void* addr, uint32_t size);
void RemovePages(uint32_t table_idx, uint32_t start_page, uint32_t num_pages);
uint32_t AddStack();