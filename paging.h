#pragma once

#include <stdint.h>
#include "paging_bootstrap.h"

extern uint32_t __total_pages;

void AddGuardPage(uint32_t Tidx, uint32_t Pidx);
uint32_t PageDirAddrV();
void InitPaging();
uint32_t AddKernelPages(uint32_t num_pages);