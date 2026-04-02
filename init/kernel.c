#include "kernel.h"

void kmain() {
    start();

    superblock_t* sb = EXT2MountRoot();

    dentry_t dentry;
    dentry.name = "test.txt";
    dentry.inode = NULL;

    EXT2Lookup(sb->root_inode, &dentry);

    EXT2Umount(sb);
    end();
}