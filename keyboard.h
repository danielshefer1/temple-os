#pragma once

#include "includes.h"
#include "defintions.h"
#include "global.h"
#include "vga.h"
#include "slab_alloc.h"

void PushKeyboardBuffer(InputBuffer* buffer, char c);
void InitConsoleBuffer();
void kscanf(const char *format, ...);
void FlushBuffer(InputBuffer* buffer);
uint32_t GetInputUntilKey(InputBuffer* buffer, char* user_buffer, uint32_t max_read, uint32_t ms_back, Tuple* keys);