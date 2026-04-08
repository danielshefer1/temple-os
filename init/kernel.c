#include "kernel.h"

void kmain() {
    start();
    superblock_t* sb = EXT2MountRoot();

    dentry_t dentry = {
        .name = "test",
        .inode = NULL,
    };

    EXT2Create(sb->root_inode, &dentry, 0644);

    sb->ops->unmount(sb);
    end();
}