 #include "kernel.h"


void start() {
    SetGDT();
    InitPaging();
    SetFirstTSS();
    InitIDT();


    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc((KERNEL_VIRTUAL >> 1) + PAGE_SIZE, KERNEL_VIRTUAL - PAGE_SIZE);


    InitTimer(TIMER_FREQUENCY);
    InitVGA();
    InitConsoleBuffer();
    kprintf("Kernel Initialized Successfully\n");
}


void end() {
    CliHelper();
    HltHelper();
}


void kmain() {
    start();


    RequestBuddy(pow(2, 30));

    switch_to_user_mode(0x1000 , 0x3000);

    end();
}