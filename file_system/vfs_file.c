#include "vfs.h"
#include "devfs.h"
#include "pipe.h"

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
    // O_APPEND: each write seeks to end-of-file first. Lets `>>` redirection
    // accumulate output without racing with concurrent writers (POSIX semantics).
    if (f->flags & O_APPEND) {
        (void) VFS_CALL(f->ops, seek, f, 0, SEEK_END);
    }
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

int64_t vfs_open(inode_t* in, file_t* f, uint32_t flags) {
    int64_t r = vfs_check_inode(in);
    if (r < 0) return r;
    if (f == NULL) return -EINVAL;

    // FIFOs: route through the in-kernel pipe code. The on-disk inode has
    // no useful file_ops (it stores no data on disk); we replace it with
    // the bidirectional pipe_fops and lazily create a pipe_t cached on
    // the inode so multiple openers share a single ring buffer.
    if (in->type == VFS_TYPE_FIFO) {
        return fifo_open(in, f, flags);
    }

    // Special files: dispatch through devfs. The on-disk inode's file_ops
    // (from the underlying fs, e.g. ext2) doesn't know how to talk to a
    // tty — we replace the per-inode fops with the registered driver's,
    // and stash the driver's token so reads/writes have device context.
    if (in->type == VFS_TYPE_CHARDEV || in->type == VFS_TYPE_BLOCKDEV) {
        bool is_block = (in->type == VFS_TYPE_BLOCKDEV);
        devfs_entry_t* dev = devfs_lookup(is_block, MAJOR(in->dev_id), MINOR(in->dev_id));
        if (dev == NULL) return -ENODEV;

        f->inode = in;
        f->ops = dev->fops;
        f->position = 0;
        f->flags = 0;
        f->mode = 0;
        f->ref_count = 1;
        f->private_data = dev->token;
        // Driver-side open is optional; many character devices have no
        // per-open state to initialize beyond what we just set.
        if (dev->fops->open == NULL) return 0;
        return dev->fops->open(in, f);
    }

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
        if (f->dirent_stash != NULL) {
            kfree(f->dirent_stash, f->dirent_stash_len);
            f->dirent_stash = NULL;
        }
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

// linux_dirent64 layout, mirrored on the kernel side. Keep the field types
// in sync with user/sys/dirent.h.
typedef struct __attribute__((packed)) {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];
} kdirent64_t;

#define DIRENT_ALIGN(x) (((x) + 7) & ~((uint64_t)7))

static uint8_t vfs_type_to_dt(uint64_t vfs_type) {
    switch (vfs_type) {
        case VFS_TYPE_FILE:     return 8;   // DT_REG
        case VFS_TYPE_DIR:      return 4;   // DT_DIR
        case VFS_TYPE_SYMLINK:  return 10;  // DT_LNK
        case VFS_TYPE_CHARDEV:  return 2;   // DT_CHR
        case VFS_TYPE_BLOCKDEV: return 6;   // DT_BLK
        case VFS_TYPE_FIFO:     return 1;   // DT_FIFO
        case VFS_TYPE_SOCKET:   return 12;  // DT_SOCK
        default:                return 0;   // DT_UNKNOWN
    }
}

// Encode one entry into `dst` (at least `cap` bytes); returns the record's
// 8-byte-aligned reclen, or 0 if the entry won't fit.
static uint64_t encode_dirent(void* dst, uint64_t cap,
                              const char* name, uint64_t ino, uint8_t type) {
    uint64_t namelen = strlen(name);
    uint64_t reclen  = DIRENT_ALIGN(sizeof(kdirent64_t) + namelen + 1);
    if (reclen > cap) return 0;

    kdirent64_t* e = (kdirent64_t*)dst;
    e->d_ino    = ino;
    e->d_off    = (int64_t)reclen;   // we don't track absolute offsets; deltas suffice for iterators that just walk d_reclen
    e->d_reclen = (uint16_t)reclen;
    e->d_type   = type;
    memcpy(e->d_name, name, namelen);
    e->d_name[namelen] = '\0';
    // Zero the padding bytes so the encoded record is fully initialised.
    for (uint64_t i = sizeof(kdirent64_t) + namelen + 1; i < reclen; i++) {
        ((char*)dst)[i] = 0;
    }
    return reclen;
}

int64_t vfs_getdents(file_t* f, void* buf, uint64_t size) {
    int64_t r = vfs_check_file(f);
    if (r < 0) return r;
    if (f->inode->type != VFS_TYPE_DIR) return -ENOTDIR;
    if (buf == NULL || size == 0) return -EINVAL;

    uint64_t out = 0;
    char* dst = (char*)buf;

    // 1. Drain any stashed entry from a prior overflowed call. The stash
    //    is exactly one already-encoded record; if it doesn't fit even now
    //    the caller's buffer is too small for this directory.
    if (f->dirent_stash != NULL) {
        if (f->dirent_stash_len > size) return -EINVAL;
        memcpy(dst, f->dirent_stash, f->dirent_stash_len);
        out += f->dirent_stash_len;
        kfree(f->dirent_stash, f->dirent_stash_len);
        f->dirent_stash = NULL;
        f->dirent_stash_len = 0;
    }

    // 2. Pull entries from the underlying readdir, encoding each into the
    //    user buffer until one doesn't fit.
    while (1) {
        dentry_t entry;
        memset(&entry, 0, sizeof(entry));
        int64_t rd = VFS_CALL(f->ops, readdir, f, &entry);
        if (rd < 0) return rd;
        if (rd == 0) break;          // end-of-directory

        uint64_t ino  = (entry.inode != NULL && entry.inode->fs_specific != NULL)
                           ? *(uint32_t*)entry.inode->fs_specific
                           : 0;
        uint8_t  type = (entry.inode != NULL)
                           ? vfs_type_to_dt(entry.inode->type)
                           : 0;

        uint64_t reclen = encode_dirent(dst + out, size - out,
                                        entry.name, ino, type);
        if (reclen == 0) {
            // Doesn't fit. Encode it into a stash buffer so the *next*
            // getdents call delivers it before resuming.
            uint64_t namelen = strlen(entry.name);
            uint64_t need    = DIRENT_ALIGN(sizeof(kdirent64_t) + namelen + 1);
            if (out == 0) {
                // Caller's buffer can't hold even one entry from this dir.
                if (entry.inode != NULL) vfs_iput(entry.inode);
                vfs_strfree(entry.name);
                return -EINVAL;
            }
            void* stash = kmalloc(need);
            if (stash == NULL) {
                if (entry.inode != NULL) vfs_iput(entry.inode);
                vfs_strfree(entry.name);
                return -ENOMEM;
            }
            encode_dirent(stash, need, entry.name, ino, type);
            f->dirent_stash = stash;
            f->dirent_stash_len = need;

            if (entry.inode != NULL) vfs_iput(entry.inode);
            vfs_strfree(entry.name);
            break;
        }

        out += reclen;
        if (entry.inode != NULL) vfs_iput(entry.inode);
        vfs_strfree(entry.name);
    }

    return (int64_t)out;
}