#include "pci.h"

void PciEnumeration() {
    for (uint64_t bus = 0; bus < 256; bus++) {
        for (uint64_t dev = 0; dev < 32; dev++) {
            for (uint64_t func = 0; func < 8; func++) {
                
                uint64_t device_phys = (uint64_t)ecam_ptr + ((bus << 20) | (dev << 15) | (func << 12));


                map_page_to_virt(PCI_SCAN_VIRTUAL, device_phys, RW_MMIO, false);

                volatile pci_config_t* config = (pci_config_t*)PCI_SCAN_VIRTUAL;

                // 3. Check if a device is actually there
                if (config->vendor_id == 0xFFFF) {
                    continue; // Skip empty slots
                }

                // 4. Identify the device
                //kprintf("Found PCI Device: %x:%x | Class: %x Sub: %x\n", 
                        //config->vendor_id, config->device_id, config->class_code, config->subclass);

                // 5. Look for your Advanced Disk (AHCI)
                if (config->class_code == AHCI_CLASS && config->subclass == AHCI_SUBCLASS) {
                    kprintf("Found AHCI device!\n");
                    InitPcieAhci(config);
                }

                // 6. Look for your xHCI (USB 3.0)
                if (config->class_code == xHCI_CLASS && config->subclass == xHCI_SUBCLASS) {
                    kprintf("Found xHCI device!\n");
                }
                
                // Optimization: If not a multi-function device, don't scan funcs 1-7
                if (func == 0 && !(config->header_type & 0x80)) {
                    break;
                }
            }
        }
    }

}

void InitPcieAhci(pci_config_t* config) {
    uint64_t bar5 = get_pci_bar(config, 5);
    kprintf("AHCI BAR5: %x\n", bar5);

    hba = (hba_mem_t*) AHCI_VIRTUAL;
    uint32_t bar5_size = get_bar_size(config, 5);
    kprintf("AHCI BAR5 Size: %x\n", bar5_size);

    uint32_t bar5_num_pages = (bar5_size + 0x1000 - 1) / 0x1000; 
    for (uint32_t i = 0; i < bar5_num_pages; i++) {
        map_page_to_virt(AHCI_VIRTUAL + i * 0x1000, bar5 + i * 0x1000, RW_MMIO, false);
    }

    pci_enable_msi(config, AHCI_INT_VECTOR);
}

uint64_t get_pci_bar(pci_config_t* dev, int bar_index) {
    uint32_t low = dev->bars[bar_index];
    if ((low & 0x6) == 0x4) {
        uint32_t high = dev->bars[bar_index + 1];
        return ((uint64_t)high << 32) | (low & ~0xF);
    }
    return (low & ~0xF); // Mask out the flags in the bottom 4 bits
}

uint32_t get_bar_size(pci_config_t* dev, int bar_index) {
    uint32_t original_val = dev->bars[bar_index];

    dev->bars[bar_index] = 0xFFFFFFFF;

    uint32_t masked_val = dev->bars[bar_index];

    dev->bars[bar_index] = original_val;

    uint32_t size = masked_val & ~0xF;
    size = ~size + 1;

    return size;
}

uint8_t find_capability(pci_config_t* dev, uint8_t cap_id) {
    if ((dev->status & 0x10) == 0) {
        return 0; 
    }
    uint8_t ptr = dev->capabilities_ptr;
    while (ptr != 0) {
        uint8_t* cap = (uint8_t*)dev + ptr;
        if (cap[0] == cap_id) {
            return ptr;
        }
        ptr = cap[1]; 
    }
    return 0; 
}

void pci_enable_msi(pci_config_t* dev, uint8_t vector) {
    uint8_t cap_ptr = find_capability(dev, MSI_CAP); 
    if (cap_ptr == 0) {
        kprintf("Device does not support MSI!\n");
        return;
    } 

    msi_cap_t* msi = (msi_cap_t*)((uint8_t*)dev + cap_ptr);

    msi->message_addr = 0xFEE00000;
    
    if (msi->message_ctl & (1 << 7)) { 
        msi->message_addr_u = 0;
        msi->message_data = vector;
    } else {
        uint16_t* data32 = (uint16_t*)((uint8_t*)msi + 0x08);
        *data32 = vector;
    }

    msi->message_ctl |= 0x0001;
}