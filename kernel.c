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

    pde_t* pd_main = PageDirAddrV();

    uint8_t user_proc_arr[80];

    PrintBuddyBin(0, MAX_ORDER);
    uint32_t user_proc_addr = (uint32_t) RequestBuddy(0x40000000);
    pte_t* pt = (pte_t*) ((pd_main[0].frame << 12) + KERNEL_VIRTUAL);

    uint32_t Virtual_user_proc_addr = user_proc_addr - (KERNEL_VIRTUAL >> 1);
    uint8_t* user_proc = (uint8_t*) Virtual_user_proc_addr;
    kprintf("The returned Address is: %x", user_proc);
    for (uint32_t i = 0; i < 80; i++) {
        user_proc_arr[i] = user_proc[i];
    }



    end();
}