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
            return;
        }
    }
}

void InitRsdt() {
    FindRsdp();
    FillPageDirectoryMMIO((void*)MMIO_BASE, TABLE_SIZE);
    FindRsdt();
    FindMadt(rsdt);
}

void InitApic() {
    ParseMadt(madt);
}