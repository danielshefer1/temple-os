#pragma once

#include "includes.h"
#include "vga.h"
#include "paging.h"
#include "buddy_alloc.h"
#include "slab_alloc.h"
#include "math_ops.h"
#include "set_gdt.h"
#include "set_idt.h"
#include "timer.h"
#include "keyboard.h"
#include "E820.h"
#include "set_tss.h"

void kmain();