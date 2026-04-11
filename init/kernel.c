#include "kernel.h"

void kmain() {
    start();
    superblock_t* sb = EXT2MountRoot();

    dentry_t dir = {
        .name = "test1",
        .inode = NULL,
    };

    EXT2Lookup(sb->root_inode, &dir);

    dentry_t file = {
        .name = "test_file",
        .inode = NULL,
    };
    EXT2Lookup(dir.inode, &file);

    EXT2Unlink(dir.inode, &file);

    sb->ops->unmount(sb);
    end();
}