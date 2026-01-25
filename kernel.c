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

void VFS_Test() {
    dentry_t *mount1, *folder1, *folder2, *file1, *sys_link1, *file2, *hard_link1;
    folder1 = VFS_CreateDentry("folder1", "/", VFS_DIRECTORY, NULL);
    mount1 = VFS_Mount("mount1", "/", NULL, folder1);
    folder2 = VFS_CreateDentry("folder2", "/mount1", VFS_DIRECTORY, NULL);
    file1 = VFS_CreateDentry("file1", "folder2", VFS_FILE, folder1);
    sys_link1 = VFS_SysLink("sys-link1", "/", folder1, "/folder1");
    file2 = VFS_CreateDentry("file2", ".", VFS_FILE, sys_link1);
    hard_link1 = VFS_HardLink("file1_cpy", "/", NULL, NULL, "/folder1/folder2/file1");
    VFS_RemoveDentry(folder2);
    PrintVFS_Root();
}


void kmain() {
    start();

    VFS_Test();

    end();
}