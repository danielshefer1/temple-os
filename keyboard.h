#pragma once

#include "includes.h"
#include "extern.h"
#include "defintions.h"
#include "global.h"
#include "vga.h"
#include "slab_alloc.h"

void PushKeyboardBuffer(input_buffer_t* buffer, char c);
void InitConsoleBuffer();
void kscanf(const char *format, ...);
void FlushBuffer(input_buffer_t* buffer);
uint64_t GetInputUntilKey(input_buffer_t* buffer, char* user_buffer, uint64_t max_read, uint64_t ms_back, tuple_t* keys);