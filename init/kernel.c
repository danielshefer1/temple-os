#include "kernel.h"

void kmain() {
    start();

    superblock_t* sb = EXT2MountRoot();

    dentry_t file = {
        .name = "file",
        .inode = NULL,
    };

    //bclean(sb);

    EXT2Lookup(sb->root_inode, &file);

    sb->ops->unmount(sb);
    end();
}