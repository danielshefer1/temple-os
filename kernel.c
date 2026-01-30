 #include "kernel.h"

void kmain() {
    start();

    uint32_t test;
    kprintf("Just checking if kscanf is still working: ");
    kscanf("%d", &test);
    kprintf("Input was %d", test);

    end();
}