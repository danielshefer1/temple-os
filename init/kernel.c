 #include "kernel.h"

void kmain() {
    start();
    
    ParseDevicesMbrs();

    superblock_t* sb = Fat32MountRootWrapper();
    kprintf("Superblock's address: %x\n", sb);
    if (sb != NULL) kprintf("Mounted successfully using sb!");

    end();
}