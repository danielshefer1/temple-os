#pragma once

#include "types.h"
#include "defintions.h"
#include "bootstrap_includes.h"

extern void enable_long_mode_and_jump(page_entry_t* pml4);

void InitPaging();    
page_entry_t* GetPML4();