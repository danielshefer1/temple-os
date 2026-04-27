#include "vfs.h"
#include "ext2_sb_ops.h"

dentry_t* vfs_root = NULL;

int64_t vfs_mount_root(superblock_t* sb) {
    if (vfs_root != NULL) return -EBUSY;
    int64_t r = vfs_check_sb(sb);
    if (r < 0) return r;
    if (sb->root_inode == NULL) return -EINVAL;

    dentry_t* root = (dentry_t*) kmalloc(sizeof(dentry_t));
    if (root == NULL) return -ENOMEM;
    memset(root, 0, sizeof(dentry_t));

    root->name = vfs_strdup("/");
    if (root->name == NULL) {
        kfree(root, sizeof(dentry_t));
        return -ENOMEM;
    }
    root->parent = root;
    root->inode = sb->root_inode;
    root->mount_type = MOUNT_FILESYSTEM;

    vfs_root = root;
    return 0;
}

int64_t vfs_unmount_root(void) {
    if (vfs_root == NULL) return -EINVAL;
    if (vfs_root->inode == NULL || vfs_root->inode->sb == NULL) return -EINVAL;

    superblock_t* sb = vfs_root->inode->sb;
    int64_t r = vfs_sync(sb);
    if (r < 0) return r;
    r = vfs_unmount(sb);
    if (r < 0) return r;

    vfs_strfree(vfs_root->name);
    kfree(vfs_root, sizeof(dentry_t));
    vfs_root = NULL;
    return 0;
}

dentry_t* vfs_traverse_mount(dentry_t* d) {
    while (d != NULL && d->mount_type == MOUNT_FILESYSTEM && d->mount_dentry != NULL) {
        d = d->mount_dentry;
    }
    return d;
}
