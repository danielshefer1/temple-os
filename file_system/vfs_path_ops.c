#include "vfs.h"

// Resolve `path` to (parent, leaf), allocate a fresh dentry under parent,
// and call `op` on it. On failure, free the dentry. Used by create/mkdir/symlink.
typedef int64_t (*parent_op_t)(inode_t* dir, dentry_t* d, void* arg);

static int64_t with_new_child(const char* path, parent_op_t op, void* arg,
                              dentry_t** out_dentry) {
    char leaf[VFS_NAME_MAX + 1];
    dentry_t* parent = NULL;
    int64_t r = vfs_namei_parent(path, &parent, leaf, sizeof(leaf));
    if (r < 0) return r;
    if (parent == NULL || parent->inode == NULL) return -ENOENT;

    dentry_t* d = vfs_dentry_alloc(parent, leaf);
    if (d == NULL) return -ENOMEM;

    r = op(parent->inode, d, arg);
    if (r < 0) {
        vfs_dentry_free(d);
        return r;
    }
    dCachePut(d);
    if (out_dentry) *out_dentry = d;
    return 0;
}

// Resolve to (parent, existing-child); call `op`. Used by unlink/rmdir.
static int64_t with_existing_child(const char* path,
                                   int64_t (*op)(inode_t* dir, dentry_t* d)) {
    char leaf[VFS_NAME_MAX + 1];
    dentry_t* parent = NULL;
    int64_t r = vfs_namei_parent(path, &parent, leaf, sizeof(leaf));
    if (r < 0) return r;
    if (parent == NULL || parent->inode == NULL) return -ENOENT;

    dentry_t* d = vfs_dentry_get(parent, leaf);
    if (d == NULL) return -ENOENT;

    r = op(parent->inode, d);
    if (r < 0) return r;
    vfs_dentry_free(d);
    return 0;
}

// ---- thin op-callback adapters (so with_new_child can dispatch generically) ----
static int64_t op_create  (inode_t* dir, dentry_t* d, void* a) { return vfs_create(dir, d, *(uint64_t*)a); }
static int64_t op_mkdir   (inode_t* dir, dentry_t* d, void* a) { return vfs_mkdir (dir, d, *(uint64_t*)a); }
static int64_t op_symlink (inode_t* dir, dentry_t* d, void* a) { return vfs_symlink(dir, d, (const char*)a); }

// ---- public API ----

int64_t vfs_create_path(const char* path, uint64_t perm) {
    return with_new_child(path, op_create, &perm, NULL);
}

int64_t vfs_mkdir_path(const char* path, uint64_t perm) {
    return with_new_child(path, op_mkdir, &perm, NULL);
}

int64_t vfs_symlink_path(const char* target, const char* linkpath) {
    if (target == NULL) return -EINVAL;
    return with_new_child(linkpath, op_symlink, (void*)target, NULL);
}

int64_t vfs_unlink_path(const char* path) {
    return with_existing_child(path, vfs_unlink);
}

int64_t vfs_rmdir_path(const char* path) {
    return with_existing_child(path, vfs_rmdir);
}

int64_t vfs_rename_path(const char* old_path, const char* new_path) {
    char old_leaf[VFS_NAME_MAX + 1], new_leaf[VFS_NAME_MAX + 1];
    dentry_t* old_parent = NULL;
    dentry_t* new_parent = NULL;

    int64_t r = vfs_namei_parent(old_path, &old_parent, old_leaf, sizeof(old_leaf));
    if (r < 0) return r;
    r = vfs_namei_parent(new_path, &new_parent, new_leaf, sizeof(new_leaf));
    if (r < 0) return r;
    if (old_parent == NULL || old_parent->inode == NULL ||
        new_parent == NULL || new_parent->inode == NULL) return -ENOENT;

    dentry_t* old_d = vfs_dentry_get(old_parent, old_leaf);
    if (old_d == NULL) return -ENOENT;

    dentry_t* new_d = vfs_dentry_alloc(new_parent, new_leaf);
    if (new_d == NULL) return -ENOMEM;

    r = vfs_rename(old_parent->inode, old_d, new_parent->inode, new_d);
    if (r < 0) {
        vfs_dentry_free(new_d);
        return r;
    }
    dCachePut(new_d);
    vfs_dentry_free(old_d);
    return 0;
}

int64_t vfs_readlink_path(const char* path, char* buf, uint64_t size) {
    dentry_t* d = NULL;
    int64_t r = vfs_namei(path, &d);
    if (r < 0) return r;
    if (d == NULL || d->inode == NULL) return -ENOENT;
    return vfs_readlink(d->inode, buf, size);
}

int64_t vfs_stat_path(const char* path, fs_stat_t* out) {
    dentry_t* d = NULL;
    int64_t r = vfs_namei(path, &d);
    if (r < 0) return r;
    if (d == NULL || d->inode == NULL) return -ENOENT;
    return vfs_getattr(d->inode, out);
}

int64_t vfs_open_path(const char* path, uint32_t flags, uint64_t mode,
                      file_t** out) {
    if (out == NULL) return -EINVAL;

    dentry_t* d = NULL;
    int64_t r = vfs_namei(path, &d);

    if (r == -ENOENT && (flags & O_CREAT)) {
        r = vfs_create_path(path, mode);
        if (r < 0) return r;
        r = vfs_namei(path, &d);
    }
    if (r < 0) return r;
    if (d == NULL || d->inode == NULL) return -ENOENT;

    file_t* f = vfs_file_alloc();
    if (f == NULL) return -ENOMEM;

    r = vfs_open(d->inode, f);
    if (r < 0) {
        kfree(f, sizeof(file_t));
        return r;
    }
    f->flags = flags;
    f->mode = (uint32_t) mode;

    if (flags & O_TRUNC) {
        int64_t tr = vfs_truncate(f, 0);
        if (tr < 0) {
            vfs_file_put(f);
            return tr;
        }
    }
    *out = f;
    return 0;
}
