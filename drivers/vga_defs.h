#pragma once
#include "includes.h"
#include "paging_defs.h"

#define VGA_BUFFER ((volatile char*)((uint64_t)KERNEL_VIRTUAL + 0xB8000))
#define GREY_COLOR 0x07
#define RED_COLOR 0x04
#define CURSOR_START 14
#define CURSOR_END 15
#define SPACE_CHAR 0x20
