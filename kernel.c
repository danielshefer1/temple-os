 #include "kernel.h"


void start() {
    SetGDT();
    InitPaging();
    SetFirstTSS();
    InitIDT();


    InitSlabAlloc(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc((KERNEL_VIRTUAL >> 1) + PAGE_SIZE, KERNEL_VIRTUAL - 0x200000);


    InitTimer(TIMER_FREQUENCY);
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

    VFSAttr test_attr = {VFS_DIRECTORY, 0, 0, 0, 0, 0, false};
    AddVFSNode(&test_attr, "test1", "/");
    VFSAttr test1_attr = {VFS_FILE, 0, 0, 0, 0, 0, false};
    AddVFSNode(&test1_attr, "test2", "/test1");
    VFSAttr test2_attr = {VFS_FILE, 0, 0, 0, 0, 0, false};
    AddVFSNode(&test2_attr, "test3", "/");
    PrintVFSRoot();

    end();
}