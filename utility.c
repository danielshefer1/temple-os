#include "utility.h"

void start() {
    SetGDT();
    InitPaging();
    InitIDT();
    enable_sse();

    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    e820_info_t* e820_info = init_E820(E820_ADDRESS);
    InitBuddyAlloc(e820_info);

    InitConsoleBuffer();

    InitRsdt();
    InitMadt();
    InitMcfg();
    EnableLapic();
    InitTimer(TIMER_TICK_PER_MS);

    InitKeyboard();
    InitVFS();

    PciEnumeration();
    AhciInit();

    BootCores();

    sleep(100); // Wait for APs to finish initializing

    InitVGA();
    kprintf("Kernel Initialized Successfully\n");
}


void end() {
    while (true) {
        CliHelper();
        HltHelper();
    }
}