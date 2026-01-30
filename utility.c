#include "utility.h"

void start() {
    SetGDT();
    InitPaging();
    SetFirstTSS();
    InitIDT();

    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc((KERNEL_VIRTUAL >> 1) + PAGE_SIZE, KERNEL_VIRTUAL - 0x200000);

    InitRsdt();
    InitMadt();
    EnableLapic();
    InitTimer(TIMER_TICK_PER_MS);



    InitVGA();
    InitConsoleBuffer();

    InitVFS();

    kprintf("Kernel Initialized Successfully\n");
}


void end() {
    while (true) {
        CliHelper();
        HltHelper();
    }
}