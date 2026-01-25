 #include "kernel.h"


void start() {
    SetGDT();
    InitPaging();
    SetFirstTSS();
    InitIDT();


    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc((KERNEL_VIRTUAL >> 1) + PAGE_SIZE, KERNEL_VIRTUAL - 0x200000);

    InitVGA();
    InitConsoleBuffer();

    InitVFS();

    kprintf("Kernel Initialized Successfully\n");
}


void end() {
    CliHelper();
    HltHelper();
}

void kmain() {
    start();

    kprintf("MMIO_OFFSET: %x\n", MMIO_OFFSET);

    FillPageDirectoryMMIO(MMIO_BASE, TABLE_SIZE);

    FindRsdp();
    PrintRsdp();
    FindRsdt();
    rsdt_t* rsdt_main = GetRsdt();
    end();
}