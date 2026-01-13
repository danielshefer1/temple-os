#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "vga.h"

void InitIDT();
void InitTimer(uint32_t frequency);