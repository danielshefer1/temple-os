#include "kernel.h"

void kmain() {
    start();
    superblock_t* sb = EXT2MountRoot();

    dentry_t dentry;
    dentry.name = "test2";
    dentry.inode = NULL;

    EXT2Mkdir(sb->root_inode, &dentry, 0644);

    sb->ops->unmount(sb);
    end();
}