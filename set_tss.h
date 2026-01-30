#pragma once

#include "includes.h"
#include "types.h"
#include "set_gdt.h"
#include "paging.h"

void SetFirstTSS();
void SetNewTss(uint8_t core_id);