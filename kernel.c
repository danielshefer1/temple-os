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

    char test[50];
    kprintf("Enter a Phrase: ");
    kscanf("%s", test);
    kprintf("I like %s too!", test);

    end();
}