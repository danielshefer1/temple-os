#include "kernel.h"

void kmain() {
    start();

    superblock_t* sb = EXT2MountRoot();

    inode_t* test_inode = EXT2AllocInode(sb);
    ext2_inode_data_t* data = (ext2_inode_data_t*) test_inode->fs_specific;
    data->inode_number = 12;
    EXT2ReadInode(test_inode);

    EXT2Umount(sb);
    end();
}