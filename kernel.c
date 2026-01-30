 #include "kernel.h"




void kmain() {
    start();

    kprintf("Waiting ten seconds...\n");
    WaitSeconds(10);
    kprintf("Finished waiting!");

    end();
}