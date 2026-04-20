#include "kernel.h"

void kmain() {
    start();

    superblock_t* sb = EXT2MountRoot();

    dentry_t dir = {
        .name = "dir"
    };

    dentry_t file = {
        .name = "file"
    };
    dentry_t new_file = {
        .name = "new_file"
    };

    EXT2Lookup(sb->root_inode, &dir);
    EXT2Rmdir(sb->root_inode, &dir);


    sb->ops->unmount(sb);
    end();
}