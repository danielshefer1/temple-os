#include "vfs.h"

// shared check for read/write payload args
static int64_t check_io(file_t* f, const void* buf, uint64_t size) {
    int64_t r = vfs_check_file(f);
    if (r < 0) return r;
    if (f->inode->type == VFS_TYPE_DIR) return -EISDIR;
    if (size > 0 && buf == NULL) return -EINVAL;
    if (UINT64_MAX - size < f->position) return -EINVAL;
    return 0;
}

int64_t vfs_read(file_t* f, void* buf, uint64_t size) {
    int64_t r = check_io(f, buf, size);
    if (r < 0) return r;
    return VFS_CALL(f->ops, read, f, buf, size);
}

int64_t vfs_write(file_t* f, const void* buf, uint64_t size) {
    int64_t r = check_io(f, buf, size);
    if (r < 0) return r;
    r = vfs_check_writable(f->inode);
    if (r < 0) return r;
    return VFS_CALL(f->ops, write, f, buf, size);
}

int64_t vfs_seek(file_t* f, int64_t off, int64_t whence) {
    int64_t r = vfs_check_file(f);
    if (r < 0) return r;
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) return -EINVAL;
    return VFS_CALL(f->ops, seek, f, off, whence);
}

int64_t vfs_truncate(file_t* f, uint64_t new_size) {
    int64_t r = vfs_check_file(f);
    if (r < 0) return r;
    if (f->inode->type == VFS_TYPE_DIR) return -EISDIR;
    r = vfs_check_writable(f->inode);
    if (r < 0) return r;
    return VFS_CALL(f->ops, truncate, f, new_size);
}

int64_t vfs_readdir(file_t* f, dentry_t* out) {
    int64_t r = vfs_check_file(f);
    if (r < 0) return r;
    if (f->inode->type != VFS_TYPE_DIR) return -ENOTDIR;
    if (out == NULL) return -EINVAL;
    return VFS_CALL(f->ops, readdir, f, out);
}

int64_t vfs_open(inode_t* in, file_t* f) {
    int64_t r = vfs_check_inode(in);
    if (r < 0) return r;
    if (f == NULL) return -EINVAL;
    if (in->file_ops == NULL) return -ENOTSUP;

    f->inode = in;
    f->ops = in->file_ops;
    f->position = 0;
    f->flags = 0;
    f->mode = 0;
    f->ref_count = 1;
    f->private_data = NULL;
    return VFS_CALL(in->file_ops, open, in, f);
}

int64_t vfs_close(file_t* f) {
    int64_t r = vfs_check_file(f);
    if (r < 0) return r;
    return VFS_CALL(f->ops, close, f);
}

int64_t vfs_flush(file_t* f) {
    int64_t r = vfs_check_file(f);
    if (r < 0) return r;
    return VFS_CALL(f->ops, flush, f);
}

int64_t vfs_ioctl(file_t* f, uint64_t cmd, void* arg) {
    int64_t r = vfs_check_file(f);
    if (r < 0) return r;
    return VFS_CALL(f->ops, ioctl, f, cmd, arg);
}

// ---- file_t lifecycle ----

file_t* vfs_file_alloc(void) {
    file_t* f = (file_t*) kmalloc(sizeof(file_t));
    if (f == NULL) return NULL;
    memset(f, 0, sizeof(file_t));
    return f;
}

void vfs_file_get(file_t* f) {
    if (f == NULL) return;
    atomic_fetch_add(&f->ref_count, 1);
}

void vfs_file_put(file_t* f) {
    if (f == NULL) return;
    if (atomic_fetch_sub(&f->ref_count, 1) == 1) {
        vfs_close(f);
        kfree(f, sizeof(file_t));
    }
}

int64_t vfs_iterate(file_t* f, vfs_dir_cb cb, void* ctx) {
    if (cb == NULL) return -EINVAL;
    int64_t r = vfs_check_file(f);
    if (r < 0) return r;
    if (f->inode->type != VFS_TYPE_DIR) return -ENOTDIR;

    dentry_t entry;
    int64_t ret, cb_ret;
    for (;;) {
        memset(&entry, 0, sizeof(entry));
        ret = VFS_CALL(f->ops, readdir, f, &entry);
        if (ret <= 0) return ret;        // <0 error, 0 EOD
        cb_ret = cb(&entry, ctx);

        // readdir populates entry->name (kmalloc) and entry->inode (alloc_inode);
        // release both before the next iteration regardless of cb outcome.
        if (entry.inode != NULL) vfs_iput(entry.inode);
        vfs_strfree(entry.name);

        if (cb_ret < 0) return cb_ret;
    }
}

int64_t vfs_print_entry_name_with_tab(dentry_t* dentry, void* ctx) {
    if (dentry == NULL || dentry->name == NULL) return -EINVAL;
    kprintf("%s\t", dentry->name);
    return 0;
}

int64_t vfs_ls(file_t* f) {
    return vfs_iterate(f, vfs_print_entry_name_with_tab, NULL);
}