#include "vfs.h"

dentry_t* vfs_dentry_alloc(dentry_t* parent, const char* name) {
    if (name == NULL) return NULL;
    uint64_t name_len = strlen(name);
    if (name_len == 0 || name_len > VFS_NAME_MAX) return NULL;

    dentry_t* d = (dentry_t*) kmalloc(sizeof(dentry_t));
    if (d == NULL) return NULL;
    memset(d, 0, sizeof(dentry_t));

    d->name = vfs_strdup(name);
    if (d->name == NULL) {
        kfree(d, sizeof(dentry_t));
        return NULL;
    }
    d->parent = parent;
    d->mount_type = MOUNT_NONE;
    return d;
}

void vfs_dentry_free(dentry_t* d) {
    if (d == NULL) return;
    dCacheRemove(d);
    vfs_strfree(d->name);
    kfree(d, sizeof(dentry_t));
}

dentry_t* vfs_dentry_get(dentry_t* parent, const char* name) {
    if (parent == NULL || parent->inode == NULL || name == NULL) return NULL;

    dentry_t* hit = dCacheLookup(parent, (char*) name);
    if (hit != NULL) return hit;

    dentry_t* d = vfs_dentry_alloc(parent, name);
    if (d == NULL) return NULL;

    if (vfs_lookup(parent->inode, d) < 0) {
        vfs_dentry_free(d);
        return NULL;
    }
    dCachePut(d);
    return d;
}

inode_t* vfs_iget(superblock_t* sb) {
    inode_t* in = vfs_alloc_inode(sb);
    if (in == NULL) return NULL;
    if (vfs_read_inode(in) < 0) {
        vfs_free_inode(in);
        return NULL;
    }
    return in;
}

void vfs_iput(inode_t* in) {
    if (in == NULL) return;
    vfs_free_inode(in);
}
