#pragma once

#include "includes.h"
#include "defintions.h"
#include "global.h"
#include "vga.h"
#include "slab_alloc.h"

void PushKeyboardBuffer(InputBuffer* buffer, char c);
void InitConsoleBuffer();
void scanf(const char *format, void* pointer);