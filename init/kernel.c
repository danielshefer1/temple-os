#include "kernel.h"

void kmain() {
    start();

    superblock_t* sb = EXT2MountRoot();

    dentry_t dir = {
        .name = "dir"
    };

    dentry_t new_file1 = {
        .name = "new_file_link"
    };
    dentry_t new_file = {
        .name = "new_file"
    };

    EXT2Lookup(sb->root_inode, &dir);
    EXT2Lookup(sb->root_inode, &new_file);
    EXT2HardLink(dir.inode, new_file.inode, &new_file1);


    sb->ops->unmount(sb);
    end();
}