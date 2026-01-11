#include "kernel.h"

void start() {
    clear_screen();
    InitPaging();
    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc(KERNEL_VIRTUAL >> 1, KERNEL_VIRTUAL);
    SetGDT();
    InitIDT();
    kprintf("Kernel Initialized Successfully\n");
}

void end() {
    CliHelper();
    HltHelper();
}

void kmain() {
    start();

    end();
}