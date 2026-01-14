#include "kernel.h"

void start() {
    clear_screen();
    InitPaging();
    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc(KERNEL_VIRTUAL >> 1, KERNEL_VIRTUAL);
    SetGDT();
    InitIDT();
    InitTimer(TIMER_FREQUENCY);
    //InitConsoleBuffer();
    kprintf("Kernel Initialized Successfully\n");
}

void end() {
    CliHelper();
    HltHelper();
}

void kmain() {
    start();

    uint32_t size = pow(2, 28);
    RequestBuddy(size);
    PrintBuddyBin(0, MAX_ORDER);
    end();
}