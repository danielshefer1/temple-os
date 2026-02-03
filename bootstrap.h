#pragma once

#include "types.h"
#include "defintions.h"
#include "bootstrap_includes.h"

extern uint32_t kernel_sectors;
extern void enable_long_mode_and_jump();

void InitPaging();
page_entry_t* GetPML4();