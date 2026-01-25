#include "apic.h"

RSDP_descriptor* FindRsdp() {
    for (uint32_t addr = 0xE0000 + KERNEL_VIRTUAL; addr < 0x100000 + KERNEL_VIRTUAL; addr += 16) {
        if (memcmp((void*)addr, "RSD PTR ", 8) == 0) {
            uint8_t sum = 0;
            uint8_t* bytes = (uint8_t*)addr;
            for (int i = 0; i < 20; i++) {
                sum += bytes[i];
            }

            if (sum == 0) {
                return (RSDP_descriptor*)addr;
            }
        }
    }
    return NULL; // RSDP not found
}

void PrintRsdp(RSDP_descriptor* rsdp) {
    kprintf("Oem ID: ");
    print_str_SYSCALL(rsdp->oem_id, GREY_COLOR, 6);
    kprintf("\tRevision: %d\tRSDT Address: %x", rsdp->revision, rsdp->rsdt_address);
}