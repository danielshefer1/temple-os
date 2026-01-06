#include "kernel.h"

void start() {
    clear_screen();
    kprintf("Starting Kernel\n");
    InitPaging();
    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc(KERNEL_VIRTUAL >> 1, KERNEL_VIRTUAL);
}

void end() {
    __asm__ volatile("cli");
    __asm__ volatile("hlt");
}

void kmain() {
    start();
    
    PrintBuddyBin();

    end();
}