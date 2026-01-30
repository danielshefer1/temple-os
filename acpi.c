#include "acpi.h"

static rsdp_t* rsdp;
static rsdt_t* rsdt;
static madt_t* madt;


bool ValidateRsdp(void* addr) {
    if (memcmp((void*)addr, "RSD PTR ", RSDP_SIG_LENGTH) == 0) {
        uint8_t sum = 0;
        uint8_t* bytes = (uint8_t*)addr;
        for (int i = 0; i < 20; i++) {
            sum += bytes[i];
        }

        if (sum == 0) {
            return true;
        }
    }
    return false;
}

bool ValidateACPIHeader(acpi_header_t* header) {
    uint8_t sum = 0;
    uint8_t* bytes = (uint8_t*) header;
    uint32_t length = header->length;

    for (uint32_t i = 0; i < length; i++) {
        sum += bytes[i];
    }
    
    return (sum == 0);
}

void FindRsdp() {
    for (uint32_t addr = 0xE0000 + KERNEL_VIRTUAL; addr < 0x100000 + KERNEL_VIRTUAL; addr += 16) {
        if (ValidateRsdp((void*) addr)) {
            rsdp = (rsdp_t*) addr;
            return;
        }
    }
    kprintf("Couldn't Find RSDP!");
}



void PrintRsdp() {
    kprintf("Oem ID: ");
    print_str_SYSCALL(rsdp->oem_id, GREY_COLOR, OEM_ID_LENGTH);
    kprintf("\tRevision: %d\tRSDT Address: %x", rsdp->revision, rsdp->rsdt_address);
}

void FindRsdt() {
    uint32_t phy_addr = rsdp->rsdt_address, ver_addr = phy_addr + MMIO_OFFSET;
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
    uint32_t num_entries = (rsdt->header.length - RSDT_HEADER_LENGTH) / sizeof(uint32_t);
    acpi_header_t* entry;

    for (uint32_t i = 0; i < num_entries; i++) {
        entry = (acpi_header_t*) MMIO_PHYS_TO_VIRT(rsdt->entries[i]);

        if (!ValidateACPIHeader(entry)) {
            kprintf("Found an unvalid ACPI table");
            continue;
        }
        if (strncmp(entry->signature, "APIC", ACPI_TABLE_SIG_LEGNTH) == 0) {
            madt = (madt_t*)entry;
            kprintf("Found MADT!\n");
            lapic = (volatile uint32_t*) madt->local_apic_address;
            return;
        }
    }
    kerror("Didn't find MADT!");
}

void ParseMadt(madt_t* madt) {
    madt_entry_header_t* entry = (madt_entry_header_t*)((uint32_t)madt + sizeof(madt_t));
    uint32_t end = madt->header.length + (uint32_t)madt;
    uint32_t over_idx = 0;

    while ((uint32_t)entry < end) {
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
                ioapic = (volatile uint32_t*) ioapic_obj->ioapic_address;
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
        entry = (madt_entry_header_t*)((uint32_t)entry + entry->length);
    }
    overrides_length = over_idx;
    kprintf("Finished parsing MADT succesfully!\n");
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
    kprintf("Local APIC's address is: %x\n", (uint32_t)lapic);
    kprintf("I/O APIC's address is: %x\n", (uint32_t)ioapic);
    FillPageDirectoryIdentityMapping(lapic, PAGE_SIZE);
    FillPageDirectoryIdentityMapping(ioapic, PAGE_SIZE);
}