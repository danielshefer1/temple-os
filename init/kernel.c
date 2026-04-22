#include "kernel.h"

void kmain() {
    start();

    superblock_t* sb = EXT2MountRoot();

    dentry_t dir = {
        .name = "dir"
    };

    EXT2Lookup(sb->root_inode, &dir);

    file_t file_dir_instance = {
        .dentry = &dir,
        .inode = dir.inode,
        .position = 50,
        .ref_count = 1
    };
    char buf[20];

    EXT2Read(&file_dir_instance, buf, 20);

    sb->ops->unmount(sb);
    end();
}