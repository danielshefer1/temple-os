#include "utility.h"

void start() {
    SetGDT();
    InitPaging();
    SetFirstTSS();
    InitIDT();
    enable_sse();

    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc((KERNEL_VIRTUAL >> 1) + PAGE_SIZE, KERNEL_VIRTUAL - 0x200000);

    InitVGA();
    InitConsoleBuffer();

    InitRsdt();
    InitMadt();
    EnableLapic();
    InitTimer(TIMER_TICK_PER_MS);
    InitKeyboard();
    InitVFS();

    kprintf("Kernel Initialized Successfully\n");
}


void end() {
    while (true) {
        CliHelper();
        HltHelper();
    }
}