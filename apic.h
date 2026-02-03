#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "memory.h"
#include "string.h"
#include "vga.h"
#include "global.h"
#include "set_idt.h"
#include "utility.h"

void EnableLapic();
void InitTimer(uint64_t ms);
void InitKeyboard();