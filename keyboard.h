#pragma once

#include "includes.h"
#include "defintions.h"
#include "global.h"
#include "vga.h"
#include "slab_alloc.h"

void PushKeyboardBuffer(InputBuffer* buffer, char c);
void InitConsoleBuffer();
void kscanf(const char *format, ...);