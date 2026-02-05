#pragma once

#include "includes.h"
#include "extern.h"
#include "types.h"
#include "defintions.h"
#include "memory.h"
#include "vga.h"
#include "paging.h"
#include "global.h"

void InitRsdt();
void InitMadt();
void InitMcfg();