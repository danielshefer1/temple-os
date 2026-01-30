#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "global.h"
#include "memory.h"
#include "vga.h"
#include "paging.h"
#include "timer.h"
#include "set_gdt.h"
#include "apic.h"
#include "paging_bootstrap.h"

void ap_kmain();
void BootCores();