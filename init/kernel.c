#include "kernel.h"

void kmain() {
    start();

    superblock_t* sb = EXT2MountRoot();

    dentry_t file = {
        .name = "file"
    };

    EXT2Create(sb->root_inode, &file, 0644);

    file_t file_instance = {
        .dentry = &file,
        .inode = file.inode,
        .position = 0,
        .ref_count = 0
    };
    EXT2Truncate(&file_instance, sb->block_size * 5 - sb->block_size / 2);
    
    sb->ops->unmount(sb);
    end();
}