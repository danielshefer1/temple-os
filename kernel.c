#include "kernel.h"

void start() {
    InitVGA();
    InitPaging();
    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc((KERNEL_VIRTUAL >> 1) + PAGE_SIZE, KERNEL_VIRTUAL - PAGE_SIZE);
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

    E820Info* info = init_E820(E820_ADDRESS);
    print_E820_entrys(info->entries, info->num_entries);
    end();
}