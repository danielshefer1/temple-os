#pragma once
#include "includes.h"
#include "lock_types.h"
#include "block_device_types.h"

typedef struct superblock_t {
    uint64_t magic;
    uint64_t block_size;
    uint64_t pages_in_block;

    void* fs_info;
    struct superblock_ops_t* ops;
    block_device_t* bdev;
    uint64_t start_lba;

    struct inode_t* root_inode;

} superblock_t;

typedef struct inode_t {
    uint32_t owner_id;
    uint32_t group_id;

    uint64_t flags;
    uint64_t type;
    uint64_t size;
    uint64_t permissions;
    uint32_t dev_id;          // valid for VFS_TYPE_CHARDEV/BLOCKDEV; encoded MKDEV(major, minor)

    struct inode_ops_t* ops;
    struct file_ops_t* file_ops;
    void* fs_specific;

    char* syslink_name;
    superblock_t* sb;

    mutex_t mutex;
} inode_t;

typedef enum {
    MOUNT_NONE = 0,
    MOUNT_BIND,
    MOUNT_FILESYSTEM
} mount_type_t;

typedef struct dentry_t {

    mutex_t mutex;

    mount_type_t mount_type;
    struct dentry_t* mount_dentry;

    char* name;
    inode_t* inode;
    struct dentry_t* parent;
    struct dentry_t* children;
    struct dentry_t* next;
} dentry_t;


typedef struct dcache_entry {
    dentry_t* dentry;
    struct dcache_entry* next;
} dcache_entry_t;

typedef struct {
    uint64_t free_blocks;     // free_clusters
    uint64_t last_alloc_block;
} fs_stat_t;

typedef struct file_t {
    inode_t* inode;
    struct file_ops_t* ops;

    mutex_t  lock;
    uint64_t position;

    uint32_t flags;
    uint32_t mode;

    _Atomic uint64_t ref_count;
    void* private_data;
} file_t;

typedef struct superblock_ops_t {
    // inode lifecycle
    inode_t* (*alloc_inode) (struct superblock_t* sb);
    int64_t  (*free_inode)  (inode_t* inode);
    int64_t  (*read_inode)  (inode_t* inode);
    int64_t  (*write_inode) (inode_t* inode);

    // filesystem lifecycle
    int64_t  (*mount)       (struct superblock_t* sb);
    int64_t  (*unmount)     (struct superblock_t* sb);
    int64_t  (*sync)        (struct superblock_t* sb);

    // filesystem info
    int64_t  (*stat)        (struct superblock_t* sb, fs_stat_t* stat);
} superblock_ops_t;


typedef struct inode_ops_t {
    // directory operations
    int64_t  (*lookup)      (inode_t* dir, dentry_t* dentry);
    int64_t  (*create)      (inode_t* dir, dentry_t* dentry, uint64_t permissions);
    int64_t  (*mkdir)       (inode_t* dir, dentry_t* dentry, uint64_t permissions);
    int64_t  (*rmdir)       (inode_t* dir, dentry_t* dentry);
    int64_t  (*unlink)      (inode_t* dir, dentry_t* dentry);
    int64_t  (*rename)      (inode_t* old_dir, dentry_t* old_dentry,
                             inode_t* new_dir, dentry_t* new_dentry);
    int64_t (*hardlink)     (inode_t* dir, inode_t* existing_inode, dentry_t* dentry);
    int64_t (*mknod)        (inode_t* dir, dentry_t* dentry, uint64_t type,
                             uint64_t permissions, uint32_t dev_id);

    // symlink operations
    int64_t  (*symlink)     (inode_t* dir, dentry_t* dentry, const char* target);
    int64_t  (*readlink)    (inode_t* inode, char* buf, uint64_t size);

    // inode info
    int64_t  (*getattr)     (inode_t* inode, fs_stat_t* stat);
    int64_t  (*setattr)     (inode_t* inode, fs_stat_t* stat);
} inode_ops_t;


typedef struct file_ops_t {
    // file I/O
    int64_t  (*read)        (file_t* file, void* buf, uint64_t size);
    int64_t  (*write)       (file_t* file, const void* buf, uint64_t size);
    int64_t  (*seek)        (file_t* file, int64_t offset, int64_t whence);
    int64_t  (*truncate)    (file_t* file, uint64_t new_size);

    // directory I/O
    int64_t  (*readdir)     (file_t* file, dentry_t* out);

    // file lifecycle
    int64_t  (*open)        (inode_t* inode, file_t* file);

    int64_t  (*close)       (file_t* file);
    int64_t  (*flush)       (file_t* file);

    // misc
    int64_t  (*ioctl)       (file_t* file, uint64_t cmd, void* arg);
} file_ops_t;

// directory iteration callback (used by vfs_iterate)
typedef int64_t (*vfs_dir_cb)(dentry_t* entry, void* ctx);
