#include "kernel.h"

void kmain() {
    start();

    kprintf("Input a number: ");

    uint64_t test;
    kscanf("%d", &test);

    end();
}