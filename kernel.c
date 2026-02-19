 #include "kernel.h"

void kmain() {
    start();

    AhciInit();

    uint8_t* buffer = (uint8_t*)AddNonCachableKernelPages(1);
    AhciRead(0, 0, 1, KERNEL_VIRT_TO_PHYS((uint64_t)buffer));
    
    if (buffer[510] == 0x55 && buffer[511] == 0xAA) {
        kprintf("MBR Signature Validated!\n");
    } else {
        kprintf("Invalid MBR Signature!\n");
    }
    end();
}