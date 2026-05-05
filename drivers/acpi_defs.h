#pragma once
#include "includes.h"
#include "paging_defs.h"

#define RSDP_SIG_LENGTH 8
#define OEM_ID_LENGTH 6
#define ACPI_TABLE_SIG_LEGNTH 4
#define RSDT_HEADER_LENGTH 36
#define MADT_HEADER_LENGTH 44
#define MMIO_PHYS_TO_VIRT(phys) ((uint64_t)phys + MMIO_OFFSET)
