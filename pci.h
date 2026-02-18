#pragma once

#include "includes.h"
#include "extern.h"
#include "types.h"
#include "defintions.h"
#include "paging.h"
#include "vga.h"

void PciEnumeration();
uint64_t get_pci_bar(pci_config_t* dev, int bar_index);
uint32_t get_bar_size(pci_config_t* dev, int bar_index);
void InitPcieAhci(pci_config_t* config);
void pci_enable_msi(pci_config_t* dev, uint8_t vector);