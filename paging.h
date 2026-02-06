#pragma once

#include "includes.h"
#include "extern.h"
#include "types.h"
#include "defintions.h"
#include "slab_alloc.h"

uint64_t PageDirAddrV();
void InitPaging();
uint64_t AddKernelPages(uint64_t num_pages);
uint64_t AddUserPageTable(uint64_t table_idx);
void FillUserPageTable(uint64_t table_idx, uint64_t start_page, uint64_t num_pages);
void RemovePageTables(uint64_t start_table, uint64_t end_table);
void FillPageDirectoryUser(void* addr, uint64_t size);
void FillPageDirectoryMMIO(void* addr, uint64_t size);
void FillPageDirectoryPCI(void* addr, uint64_t size);
void FillPageDirectoryIdentityMapping(void* addr, uint64_t size);
void RemovePages(uint64_t addr, uint64_t num_pages, bool big_pages);
uint64_t AddStack();
void RemovePage(uint64_t addr, bool big_page);
void map_page_to_virt(uint64_t virt, uint64_t phy, uint64_t flags, bool big_page);