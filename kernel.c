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

    RSDP_descriptor* rsdp = FindRsdp();
    PrintRsdp(rsdp);

    FillPageDirectoryMMIO(MMIO_BASE, TABLE_SIZE);

    pde_t* pd_main = PageDirAddrV();
    pte_t* pt = ((pd_main[MMIO_FIRST_TABLE].frame << 12) + KERNEL_VIRTUAL);
    end();
}