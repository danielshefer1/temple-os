#include "kernel.h"

void start() {
    SetGDT();
    InitIDT();
    InitVGA();
    InitPaging();
    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc((KERNEL_VIRTUAL >> 1) + PAGE_SIZE, KERNEL_VIRTUAL - PAGE_SIZE);
    InitTimer(TIMER_FREQUENCY);
    InitConsoleBuffer();
    kprintf("Kernel Initialized Successfully\n");
}

void end() {
    CliHelper();
    HltHelper();
}

void kmain() {
    start();

    uint32_t test;
    kprintf("Enter a Number: ");
    kscanf("%d", &test);

    end();
}