#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"

void AddGuardPage(uint32_t Tidx, uint32_t Pidx);
uint32_t PageDirAddrV();
void InitPaging();
uint32_t AddKernelPages(uint32_t num_pages);