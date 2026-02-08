#include "acpi.h"

static rsdp_t* rsdp;
static rsdt_t* rsdt;
static madt_t* madt;
static mcfg_t* mcfg;

bool ValidateRsdp(rsdp_t* rsdp) {
    // 1. Check Signature
    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        return false;
    }

    // 2. Validate Legacy Checksum (First 20 bytes)
    uint8_t sum = 0;
    uint8_t* bytes = (uint8_t*)rsdp;
    for (int i = 0; i < 20; i++) {
        sum += bytes[i];
    }
    if (sum != 0) return false;

    // 3. Validate Extended Checksum (Full 36 bytes)
    if (rsdp->revision >= 2) {
        uint8_t ext_sum = 0;
        for (int i = 0; i < 36; i++) {
            ext_sum += bytes[i];
        }
        if (ext_sum != 0) return false;
    }

    return true;
}

bool ValidateACPIHeader(acpi_header_t* header) {
    uint8_t sum = 0;
    uint8_t* bytes = (uint8_t*) header;
    uint64_t length = header->length;

    for (uint64_t i = 0; i < length; i++) {
        sum += bytes[i];
    }
    
    return (sum == 0);
}

void FindRsdp() {

    uint16_t ebda_segment = *(uint16_t*)(0x40E + KERNEL_VIRTUAL);
    uint64_t ebda_base = (uint64_t)ebda_segment << 4;

    if (ebda_base > 0) {
        for (uint64_t addr = ebda_base + KERNEL_VIRTUAL; addr < ebda_base + 1024 + KERNEL_VIRTUAL; addr += 16) {
            if (ValidateRsdp((rsdp_t*)addr)) {
                rsdp = (rsdp_t*)addr;
                return;
            }
        }
    }

    for (uint64_t addr = 0x9F400 + KERNEL_VIRTUAL; addr < 0x100000 + KERNEL_VIRTUAL; addr += 16) {
        if (ValidateRsdp((rsdp_t*) addr)) {
            rsdp = (rsdp_t*) addr;
            return;
        }
    }
    kprintf("Couldn't Find RSDP!\n");
}



void PrintRsdp() {
    kprintf("Oem ID: ");
    print_str_SYSCALL(rsdp->oem_id, GREY_COLOR, OEM_ID_LENGTH);
    kprintf("\tRevision: %d\tRSDT Address: %x", rsdp->revision, rsdp->rsdt_address);
}

void FindRsdt() {
    uint64_t phy_addr = rsdp->rsdt_address, ver_addr = phy_addr + MMIO_OFFSET;
    if (!ValidateACPIHeader((acpi_header_t*) ver_addr)) {
        kprintf("Table found is not valid!");
        return;
    }
    rsdt = (rsdt_t*) ver_addr;
    if (strncmp(rsdt->header.signature, "RSDT", ACPI_TABLE_SIG_LEGNTH) != 0) {
        kprintf("Acpi table found is not RSDT, it's ");
        print_str_SYSCALL(rsdt->header.signature, GREY_COLOR, ACPI_TABLE_SIG_LEGNTH);
        rsdt = NULL;
        return;
    }
    kprintf("RSDT found!\n");
}

void FindMadt(rsdt_t* rsdt) {
    uint64_t num_entries = (rsdt->header.length - sizeof(acpi_header_t)) / sizeof(uint32_t);
    acpi_header_t* entry;

    for (uint64_t i = 0; i < num_entries; i++) {
        uint32_t phy_addr = rsdt->entries[i];
        entry = (acpi_header_t*) MMIO_PHYS_TO_VIRT((uint64_t)phy_addr);

        if (!ValidateACPIHeader(entry)) {
            kprintf("Found an invalid ACPI table\n");
            continue;
        }
        
        if (memcmp(entry->signature, "APIC", 4) == 0) {
            madt = (madt_t*)entry;
            kprintf("Found MADT!\n");
            lapic = (volatile uint64_t*)((uint64_t)madt->local_apic_address);
            return;
        }
    }
    kerror("Didn't find MADT!");
}

void ParseMadt(madt_t* madt) {
    madt_entry_header_t* entry = (madt_entry_header_t*)((uint64_t)madt + sizeof(madt_t));
    uint64_t end = madt->header.length + (uint64_t)madt;
    uint64_t over_idx = 0;

    while ((uint64_t)entry < end) {
        switch (entry->type) {
            case 0:
                
                
                local_apic_t* ptr = (local_apic_t*) entry;
                kprintf("Cpu %d ID: %d\t", cpu_count, ptr->acpi_processor_id);
                cpu_ids[cpu_count] = ptr->acpi_processor_id;;
                cpu_count++;
                break;
            case 1:
                kprintf("Found I/O APIC!\n");
                io_apic_t* ioapic_obj = (io_apic_t*) entry;
                ioapic = (volatile uint64_t*) ioapic_obj->ioapic_address;
                break;
            case 2:
                kprintf("Found Interrupts Override!\n");
                overrides[over_idx] = (int_override_t*) entry;
                over_idx++;
                break;
            case 4:
                kprintf("Found NMI!\n");
                break;
            default:
                kprintf("Unkown MADT type: %d\n", entry->type);
        }
        entry = (madt_entry_header_t*)((uint64_t)entry + entry->length);
    }
    overrides_length = over_idx;
    kprintf("Finished parsing MADT succesfully!\n");
}

void FindMcfg(rsdt_t* rsdt) {
    uint64_t num_entries = (rsdt->header.length - RSDT_HEADER_LENGTH) / sizeof(uint32_t);
    acpi_header_t* entry;

    for (uint64_t i = 0; i < num_entries; i++) {
        entry = (acpi_header_t*) MMIO_PHYS_TO_VIRT(rsdt->entries[i]);

        if (!ValidateACPIHeader(entry)) {
            kprintf("Found an unvalid ACPI table");
            continue;
        }
        if (strncmp(entry->signature, "MCFG", ACPI_TABLE_SIG_LEGNTH) == 0) {
            mcfg = (mcfg_t*)entry;
            ecam_ptr = (pci_config_t*) mcfg->entries[0].base_address;

            kprintf("Found MCFG!\n");
            return;
        }
    }
    kerror("Didn't find MCFG!");
}

void InitRsdt() {
    FindRsdp();
    FillPageDirectoryMMIO((void*)MMIO_BASE, TABLE_SIZE);
    FindRsdt();
}

void InitMadt() {
    FindMadt(rsdt);
    ParseMadt(madt);
    kprintf("Found %d CPUs!\n", cpu_count);
    kprintf("Local APIC's address is: %x\n", (uint64_t)lapic);
    kprintf("I/O APIC's address is: %x\n", (uint64_t)ioapic);
    FillPageDirectoryIdentityMapping(lapic, PAGE_SIZE);
    FillPageDirectoryIdentityMapping(ioapic, PAGE_SIZE);
}

void InitMcfg() {
    FindMcfg(rsdt);
}