#pragma once

#include "includes.h"
#include "extern.h"
#include "types.h"
#include "defintions.h"
#include "memory.h"
#include "vga.h"
#include "paging.h"
#include "global.h"
#include "fadt.h"

void InitRsdt();
void InitMadt();
void InitMcfg();
void InitFadt();

void Shutdown();
uint8_t GetCenturyReg();