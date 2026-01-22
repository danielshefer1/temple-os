#pragma once

#include "includes.h"
#include "defintions.h"
#include "global.h"
#include "vga.h"
#include "slab_alloc.h"

void PushKeyboardBuffer(input_buffer_t* buffer, char c);
void InitConsoleBuffer();
void kscanf(const char *format, ...);
void FlushBuffer(input_buffer_t* buffer);
uint32_t GetInputUntilKey(input_buffer_t* buffer, char* user_buffer, uint32_t max_read, uint32_t ms_back, tuple_t* keys);