#include "vfs.h"

int64_t vfs_check_sb(superblock_t* sb) {
    if (sb == NULL || sb->ops == NULL) return -EINVAL;
    return 0;
}

int64_t vfs_check_inode(inode_t* in) {
    if (in == NULL || in->sb == NULL || in->ops == NULL) return -EINVAL;
    return 0;
}

int64_t vfs_check_dir(inode_t* dir) {
    int64_t r = vfs_check_inode(dir);
    if (r < 0) return r;
    if (dir->type != VFS_TYPE_DIR) return -ENOTDIR;
    return 0;
}

int64_t vfs_check_file(file_t* f) {
    if (f == NULL || f->inode == NULL || f->ops == NULL) return -EINVAL;
    return 0;
}

int64_t vfs_check_dentry(dentry_t* d) {
    if (d == NULL || d->name == NULL) return -EINVAL;
    return 0;
}

int64_t vfs_check_writable(inode_t* in) {
    if (IS_IMMUTABLE(in)) return -EPERM;
    return 0;
}

int64_t fs_io_begin(superblock_t* sb) {
    if (sb == NULL) return -EINVAL;
    mutex_lock(&sb->io_lock);
    if (sb->unmounting) {
        mutex_unlock(&sb->io_lock);
        return -EBUSY;
    }
    sb->io_inflight++;
    mutex_unlock(&sb->io_lock);
    return 0;
}

void fs_io_end(superblock_t* sb) {
    if (sb == NULL) return;
    mutex_lock(&sb->io_lock);
    if (sb->io_inflight > 0) sb->io_inflight--;
    mutex_unlock(&sb->io_lock);
}

char* vfs_strdup(const char* s) {
    if (s == NULL) return NULL;
    uint64_t len = strlen(s);
    char* out = (char*) kmalloc(len + 1);
    if (out == NULL) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

void vfs_strfree(char* s) {
    if (s == NULL) return;
    kfree(s, strlen(s) + 1);
}
