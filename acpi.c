#include "acpi.h"

static rsdp_t* rsdp;
static acpi_table_t* rsdt;

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

bool ValidateACPITable(acpi_table_t* table) {
    uint8_t sum = 0;
    uint8_t* bytes = (uint8_t*) table;
    
    for (uint32_t i = 0; i < table->header.length; i++) {
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
    if (!ValidateACPITable((acpi_table_t*) ver_addr)) {
        kprintf("Table found is not valid!");
        return;
    }
    rsdt = (acpi_table_t*) ver_addr;
    if (strncmp(rsdt->header.signature, "RSDT", ACPI_TABLE_SIG_LEGNTH) != 0) {
        kprintf("Acpi table found is not RSDT, it's ");
        print_str_SYSCALL(rsdt->header.signature, GREY_COLOR, ACPI_TABLE_SIG_LEGNTH);
        rsdt = NULL;
        return;
    }
    kprintf("RSDT found!\n");
}

void SearchRsdt() {
    if (rsdt == NULL) {
        kprintf("Find RSDT first!");
        return;
    }
    uint32_t num_entries = (rsdt->header.length - BASE_ACPI_TABLE_LENGTH) / sizeof(uint32_t);
    acpi_table_t** entrys = rsdt->entry;
    for (uint32_t i = 0; i < num_entries; i++) {

        if (!ValidateACPITable(MMIO_PHYS_TO_VIRT(entrys[i]))) {
            kprintf("Found an unvalid ACPI table");
            continue;
        }
        kprintf("Found a valid ACPI table, Signature: ");
        print_str_SYSCALL(MMIO_PHYS_TO_VIRT(entrys[i]->header.signature), GREY_COLOR, ACPI_TABLE_SIG_LEGNTH);
        putchar('\n', GREY_COLOR);
    }
}

void InitRsdt() {
    FindRsdp();
    FillPageDirectoryMMIO((void*)MMIO_BASE, TABLE_SIZE);
    FindRsdt();
    SearchRsdt();
}