#pragma once

#include "includes.h"
#include "extern.h"
#include "vga.h"
#include "paging.h"
#include "buddy_alloc.h"
#include "slab_alloc.h"
#include "set_gdt.h"
#include "set_idt.h"
#include "timer.h"
#include "keyboard.h"
#include "E820.h"
#include "vfs.h"
#include "acpi.h"
#include "apic.h"
#include "utility.h"
#include "pci.h"
#include "ahci_driver.h"
#include "mbr.h"

void kmain();