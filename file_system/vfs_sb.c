#include "vfs.h"

inode_t* vfs_alloc_inode(superblock_t* sb) {
    if (vfs_check_sb(sb) < 0) return NULL;
    if (sb->ops->alloc_inode == NULL) return NULL;
    return sb->ops->alloc_inode(sb);
}

int64_t vfs_free_inode(inode_t* in) {
    int64_t r = vfs_check_inode(in);
    if (r < 0) return r;
    return VFS_CALL(in->sb->ops, free_inode, in);
}

int64_t vfs_read_inode(inode_t* in) {
    int64_t r = vfs_check_inode(in);
    if (r < 0) return r;
    return VFS_CALL(in->sb->ops, read_inode, in);
}

int64_t vfs_write_inode(inode_t* in) {
    int64_t r = vfs_check_inode(in);
    if (r < 0) return r;
    return VFS_CALL(in->sb->ops, write_inode, in);
}

int64_t vfs_mount(superblock_t* sb) {
    int64_t r = vfs_check_sb(sb);
    if (r < 0) return r;
    return VFS_CALL(sb->ops, mount, sb);
}

int64_t vfs_unmount(superblock_t* sb) {
    int64_t r = vfs_check_sb(sb);
    if (r < 0) return r;
    return VFS_CALL(sb->ops, unmount, sb);
}

int64_t vfs_sync(superblock_t* sb) {
    int64_t r = vfs_check_sb(sb);
    if (r < 0) return r;
    return VFS_CALL(sb->ops, sync, sb);
}

int64_t vfs_stat(superblock_t* sb, fs_stat_t* out) {
    int64_t r = vfs_check_sb(sb);
    if (r < 0) return r;
    if (out == NULL) return -EINVAL;
    return VFS_CALL(sb->ops, stat, sb, out);
}
