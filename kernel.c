#include "kernel.h"

void start() {
    InitVGA();
    InitPaging();
    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc(KERNEL_VIRTUAL >> 1, KERNEL_VIRTUAL);
    SetGDT();
    InitIDT();
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
    uint32_t result;
    kprintf("Input Your Favorite Hexa Number: ");
    scanf("%x", &result);
    kprintf("Wow, %x is a Really Great Number!", result);
    end();
}