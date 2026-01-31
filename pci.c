#include "pci.h"

void InitPci() {
    FillPageDirectoryPCI(ecam_ptr, PAGE_SIZE);
    ecam_ptr = (pci_config_t*)(((uint32_t)ecam_ptr) + PCI_OFFSET);
    uint16_t vendor_id = ecam_ptr->vendor_id;
    kprintf("First Vendor ID: %x", vendor_id);
}