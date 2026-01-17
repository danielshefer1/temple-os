#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "vga.h"

void SetGDTEntry(uint32_t base, uint32_t limit, uint8_t present, uint8_t privilege, uint8_t type,
     uint8_t exec, uint8_t dir_conf, uint8_t wr, uint8_t access, uint8_t reserved,
     uint8_t long_mode, uint8_t default_big, uint8_t granularity, uint32_t idx);
void SetGDT();