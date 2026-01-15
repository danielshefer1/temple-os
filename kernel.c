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
    uint32_t num1;
    uint32_t num2;
    char c;
    char str[20];
    kprintf("Input Your Favorite 2 Numbers (And a String): ");
    kscanf("%s %d %d", str, &num1, &num2);
    kprintf("Wow, %d and %d are Really Great Numbers! (And %s is a Great String!)", num1, num2, str);
    end();
}