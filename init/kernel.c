#include "kernel.h"

void kmain() {
    start();

    superblock_t* sb = EXT2MountRoot();

    dentry_t dentry;
    dentry.name = "test1.txt";
    dentry.inode = NULL;

    EXT2Create(sb->root_inode, &dentry, 0644);

    sb->ops->unmount(sb);
    end();
}