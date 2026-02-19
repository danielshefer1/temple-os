 #include "kernel.h"

void kmain() {
    start();

    AhciInit();

    uint8_t* buffer = (uint8_t*)AddNonCachableKernelPages(1);
    AhciRead(0, 1, 1, KERNEL_VIRT_TO_PHYS((uint64_t)buffer));
    
    if (memcmp(buffer, "EFI PART", 8) == 0) {
    kprintf("Found GPT Header at LBA 1!\n");
    }
    else {
        kprintf("No GPT Header found at LBA 1, checking for MBR...\n");
        memset(buffer, 0, 512);
        AhciRead(0, 0, 1, KERNEL_VIRT_TO_PHYS((uint64_t)buffer));
        ParseMbr(buffer);
    }
    end();
}