#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "mem_ops.h"
#include "vga.h"

void FindRsdp();
void PrintRsdp();
void FindRsdt();
rsdt_t* GetRsdt();