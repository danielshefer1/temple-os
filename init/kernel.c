#include "kernel.h"

void kmain() {
    start();

    superblock_t* sb = EXT2MountRoot();

    dentry_t dir = {
        .name = "dir"
    };

    EXT2Rmdir(sb->root_inode, &dir);

    sb->ops->unmount(sb);
    end();
}