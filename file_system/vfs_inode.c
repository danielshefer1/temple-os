#include "vfs.h"

// Pair-checks shared by every directory-mutating op.
static int64_t check_dir_dentry(inode_t* dir, dentry_t* d) {
    int64_t r = vfs_check_dir(dir);
    if (r < 0) return r;
    return vfs_check_dentry(d);
}

static int64_t check_dir_dentry_writable(inode_t* dir, dentry_t* d) {
    int64_t r = check_dir_dentry(dir, d);
    if (r < 0) return r;
    return vfs_check_writable(dir);
}

int64_t vfs_lookup(inode_t* dir, dentry_t* d) {
    int64_t r = check_dir_dentry(dir, d);
    if (r < 0) return r;
    return VFS_CALL(dir->ops, lookup, dir, d);
}

int64_t vfs_create(inode_t* dir, dentry_t* d, uint64_t perm) {
    int64_t r = check_dir_dentry_writable(dir, d);
    if (r < 0) return r;
    return VFS_CALL(dir->ops, create, dir, d, perm);
}

int64_t vfs_mkdir(inode_t* dir, dentry_t* d, uint64_t perm) {
    int64_t r = check_dir_dentry_writable(dir, d);
    if (r < 0) return r;
    return VFS_CALL(dir->ops, mkdir, dir, d, perm);
}

int64_t vfs_rmdir(inode_t* dir, dentry_t* d) {
    int64_t r = check_dir_dentry_writable(dir, d);
    if (r < 0) return r;
    return VFS_CALL(dir->ops, rmdir, dir, d);
}

int64_t vfs_unlink(inode_t* dir, dentry_t* d) {
    int64_t r = check_dir_dentry_writable(dir, d);
    if (r < 0) return r;
    return VFS_CALL(dir->ops, unlink, dir, d);
}

int64_t vfs_rename(inode_t* old_dir, dentry_t* old_d,
                   inode_t* new_dir, dentry_t* new_d) {
    int64_t r = check_dir_dentry_writable(old_dir, old_d);
    if (r < 0) return r;
    r = check_dir_dentry_writable(new_dir, new_d);
    if (r < 0) return r;
    return VFS_CALL(old_dir->ops, rename, old_dir, old_d, new_dir, new_d);
}

int64_t vfs_hardlink(inode_t* dir, inode_t* existing, dentry_t* d) {
    int64_t r = check_dir_dentry_writable(dir, d);
    if (r < 0) return r;
    r = vfs_check_inode(existing);
    if (r < 0) return r;
    if (existing->type == VFS_TYPE_DIR) return -EPERM;
    return VFS_CALL(dir->ops, hardlink, dir, existing, d);
}

int64_t vfs_symlink(inode_t* dir, dentry_t* d, const char* target) {
    int64_t r = check_dir_dentry_writable(dir, d);
    if (r < 0) return r;
    if (target == NULL) return -EINVAL;
    if (strlen(target) >= VFS_PATH_MAX) return -ENAMETOOLONG;
    return VFS_CALL(dir->ops, symlink, dir, d, target);
}

int64_t vfs_readlink(inode_t* in, char* buf, uint64_t size) {
    int64_t r = vfs_check_inode(in);
    if (r < 0) return r;
    if (in->type != VFS_TYPE_SYMLINK) return -EINVAL;
    if (buf == NULL || size == 0) return -EINVAL;
    return VFS_CALL(in->ops, readlink, in, buf, size);
}

int64_t vfs_getattr(inode_t* in, fs_stat_t* out) {
    int64_t r = vfs_check_inode(in);
    if (r < 0) return r;
    if (out == NULL) return -EINVAL;
    return VFS_CALL(in->ops, getattr, in, out);
}

int64_t vfs_setattr(inode_t* in, fs_stat_t* in_stat) {
    int64_t r = vfs_check_inode(in);
    if (r < 0) return r;
    if (in_stat == NULL) return -EINVAL;
    r = vfs_check_writable(in);
    if (r < 0) return r;
    return VFS_CALL(in->ops, setattr, in, in_stat);
}
