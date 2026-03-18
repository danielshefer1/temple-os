#include "utility.h"

void start() {
    SetGDT();
    InitPaging();
    InitIDT();
    enable_sse();

    uint64_t kernel_size = (uint64_t)&__total_pages;
    InitSlabAlloc(KERNEL_VIRTUAL + kernel_size*PAGE_SIZE);
    e820_info_t* e820_info = init_E820(E820_ADDRESS);
    InitUserBuddyAlloc(e820_info);
    InitKernelBuddyAlloc(KERNEL_VIRT_TO_PHYS(GetCurrPrimitveAddr()), GB);

    InitConsoleBuffer();

    InitRsdt();
    InitMadt();
    InitMcfg();
    EnableLapic();
    InitTimer(TIMER_TICK_PER_MS);

    InitKeyboard();
    //InitVFS();

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