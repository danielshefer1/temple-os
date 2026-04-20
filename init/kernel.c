#include "kernel.h"

void kmain() {
    start();

    superblock_t* sb = EXT2MountRoot();

    dentry_t dir = {
        .name = "dir"
    };

    dentry_t symlink = {
        .name = "symlink"
    };

    char testname[2];
    EXT2Lookup(sb->root_inode, &symlink);

    EXT2ReadLink(symlink.inode, testname, sizeof(testname));

    sb->ops->unmount(sb);
    end();
}