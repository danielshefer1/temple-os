 #include "kernel.h"

void kmain() {
    start();

    ParsePci();
    kprintf("Finished parsing pci");
    
    end();
}