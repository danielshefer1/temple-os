#include "kernel.h"

void start() {
    clear_screen();
    kprintf("Starting Kernel\n");
    InitPaging();
    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc(KERNEL_VIRTUAL >> 1, KERNEL_VIRTUAL);
}

void end() {
    CliHelper();
    HltHelper();
}

void kmain() {
    start();

    PrintBuddyBin(0, MAX_ORDER);
    uint32_t test_size = pow(2, 12), test_size2 = pow(2, 16);


    kprintf("\n\n\n");
    void* test_addr = RequestBuddy(test_size);
    void* test_addr2 = RequestBuddy(test_size2);
    PrintBuddyBin(0, 20);

    FreeBuddy(test_addr);
    FreeBuddy(test_addr2);
    PrintBuddyBin(0, MAX_ORDER);

    end();
}