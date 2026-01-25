#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "mem_ops.h"
#include "vga.h"

RSDP_descriptor* FindRsdp();
void PrintRsdp(RSDP_descriptor* rsdp);