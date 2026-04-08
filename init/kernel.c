#include "kernel.h"

void kmain() {
    start();
    superblock_t* sb = EXT2MountRoot();

    dentry_t dir = {
        .name = "test1",
        .inode = NULL,
    };

    EXT2Mkdir(sb->root_inode, &dir, 0644);

    dentry_t file = {
        .name = "test_file",
        .inode = NULL,
    };

    EXT2Create(dir.inode, &file, 0644);

    sb->ops->unmount(sb);
    end();
}