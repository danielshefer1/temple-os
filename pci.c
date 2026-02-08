#include "pci.h"

void PciEnumeration() {
    for (uint64_t bus = 0; bus < 256; bus++) {
        for (uint64_t dev = 0; dev < 32; dev++) {
            for (uint64_t func = 0; func < 8; func++) {
                
                uint64_t device_phys = (uint64_t)ecam_ptr + ((bus << 20) | (dev << 15) | (func << 12));


                map_page_to_virt(PCI_SCAN_VIRTUAL, device_phys, RW_MMIO, true);

                volatile pci_config_t* config = (pci_config_t*)PCI_SCAN_VIRTUAL;

                // 3. Check if a device is actually there
                if (config->vendor_id == 0xFFFF) {
                    continue; // Skip empty slots
                }

                // 4. Identify the device
                //kprintf("Found PCI Device: %x:%x | Class: %x Sub: %x\n", 
                        //config->vendor_id, config->device_id, config->class_code, config->subclass);

                // 5. Look for your Advanced Disk (AHCI)
                if (config->class_code == 0x01 && config->subclass == 0x06) {
                    kprintf("Found AHCI device!\n");
                }
                
                // Optimization: If not a multi-function device, don't scan funcs 1-7
                if (func == 0 && !(config->header_type & 0x80)) {
                    break;
                }
            }
        }
    }

}