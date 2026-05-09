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

int64_t vfs_mount_at(dentry_t* target, superblock_t* sb) {
    if (target == NULL || target->inode == NULL) return -EINVAL;
    if (target->inode->type != VFS_TYPE_DIR) return -ENOTDIR;
    if (target->mount_dentry != NULL) return -EBUSY;

    int64_t r = vfs_check_sb(sb);
    if (r < 0) return r;
    if (sb->root_inode == NULL) return -EINVAL;

    dentry_t* root = (dentry_t*) kmalloc(sizeof(dentry_t));
    if (root == NULL) return -ENOMEM;
    memset(root, 0, sizeof(dentry_t));

    root->name = vfs_strdup(target->name ? target->name : "/");
    if (root->name == NULL) {
        kfree(root, sizeof(dentry_t));
        return -ENOMEM;
    }
    // ".." from inside the mount escapes to the mount point's parent in the
    // underlying tree — otherwise vfs_traverse_mount would re-enter the mount
    // every time the walker hit the target.
    root->parent = target->parent ? target->parent : target;
    root->inode = sb->root_inode;
    root->mount_type = MOUNT_NONE;

    target->mount_type = MOUNT_FILESYSTEM;
    target->mount_dentry = root;
    return 0;
}

int64_t vfs_unmount_at(dentry_t* target) {
    if (target == NULL || target->mount_dentry == NULL) return -EINVAL;

    dentry_t* root = target->mount_dentry;
    superblock_t* sb = (root->inode != NULL) ? root->inode->sb : NULL;
    if (sb != NULL) {
        int64_t r = vfs_unmount(sb);
        if (r < 0) return r;
    }

    target->mount_dentry = NULL;
    target->mount_type = MOUNT_NONE;
    vfs_strfree(root->name);
    kfree(root, sizeof(dentry_t));
    return 0;
}
