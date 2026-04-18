#include "kernel.h"

void kmain() {
    start();

    superblock_t* sb = EXT2MountRoot();

    dentry_t file = {
        .name = "file"
    };

    EXT2Unlink(sb->root_inode, &file);

    sb->ops->unmount(sb);
    end();
}