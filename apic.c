#include "apic.h"

static rsdp_t* rsdp;
static rsdt_t* rsdt;

void FindRsdp() {
    for (uint32_t addr = 0xE0000 + KERNEL_VIRTUAL; addr < 0x100000 + KERNEL_VIRTUAL; addr += 16) {
        if (memcmp((void*)addr, "RSD PTR ", 8) == 0) {
            uint8_t sum = 0;
            uint8_t* bytes = (uint8_t*)addr;
            for (int i = 0; i < 20; i++) {
                sum += bytes[i];
            }

            if (sum == 0) {
                rsdp = (rsdp_t*)addr;
            }
        }
    }
}

void PrintRsdp() {
    kprintf("Oem ID: ");
    print_str_SYSCALL(rsdp->oem_id, GREY_COLOR, 6);
    kprintf("\tRevision: %d\tRSDT Address: %x", rsdp->revision, rsdp->rsdt_address);
}

void FindRsdt() {
    uint32_t phy_addr = rsdp->rsdt_address, ver_addr = phy_addr + MMIO_OFFSET;
    rsdt = (rsdt_t*) ver_addr;
}

rsdt_t* GetRsdt() {
    return rsdt;
}