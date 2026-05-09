#pragma once
#include "includes.h"

#define VFS_TYPE_FILE      0x01
#define VFS_TYPE_DIR       0x02
#define VFS_TYPE_SYMLINK   0x03
#define VFS_TYPE_CHARDEV   0x04
#define VFS_TYPE_BLOCKDEV  0x05
#define VFS_TYPE_FIFO      0x06
#define VFS_TYPE_SOCKET    0x07
#define VFS_TYPE_UNKNOWN   0x00

#define EOK          0    // success
#define EPERM        1    // operation not permitted (wrong permissions)
#define ENOENT       2    // no such file or directory
#define ESRCH        3    // no such process
#define ECHILD       10   // no child processes
#define EIO          5    // I/O error (disk read/write failed)
#define EBADF        9    // bad file descriptor
#define ENOMEM       12   // out of memory
#define EACCES       13   // permission denied
#define EBUSY        16   // device or resource busy
#define EEXIST       17   // file already exists
#define ENOTDIR      20   // not a directory
#define EMFILE       24   // too many open files
#define EISDIR       21   // is a directory (tried to read a dir as a file)
#define EINVAL       22   // invalid argument
#define ENOSPC       28   // no space left on device
#define EROFS        30   // read only filesystem
#define ENOTEMPTY    39   // directory not empty (tried to rmdir non-empty dir)
#define ENOTSUP      95   // operation not supported
#define ENODEV       19   // no such device (no driver registered for inode's dev_id)
#define ENOTTY       25   // inappropriate ioctl for file
#define ELOOP        40   // too many levels of symlinks
#define ENAMETOOLONG 36   // file name too long

// open(2) flags
#define O_RDONLY     0x0000
#define O_WRONLY     0x0001
#define O_RDWR       0x0002
#define O_ACCMODE    0x0003
#define O_CREAT      0x0040
#define O_EXCL       0x0080
#define O_TRUNC      0x0200
#define O_APPEND     0x0400

// VFS limits
#define VFS_NAME_MAX     255
#define VFS_PATH_MAX     4096
#define VFS_SYMLINK_MAX  40

// seek whence
#define SEEK_SET     0   // from start of file
#define SEEK_CUR     1   // from current position
#define SEEK_END     2   // from end of file

// EXT2 ioctl commands
#define EXT2_IOC_GET_INO        1   // arg: uint32_t* — on-disk inode number
#define EXT2_IOC_GET_SIZE       2   // arg: uint64_t* — file size in bytes
#define EXT2_IOC_GET_BLOCK_SIZE 3   // arg: uint32_t* — fs block size
#define EXT2_IOC_FIBMAP         4   // arg: uint64_t* in: logical block, out: physical block
#define EXT2_IOC_SYNC_FILE      5   // arg: NULL — flush dirty blocks + write inode

#define S_SYNC        1   // Writes are synced at once
#define S_IMMUTABLE   2   // Immutable file
#define S_APPEND      4   // Append-only file
#define S_NOATIME     8   // Do not update access times
#define S_NODUMP     16   // Do not dump

#define IS_SYNC(inode)      ((inode)->flags & S_SYNC)
#define IS_IMMUTABLE(inode) ((inode)->flags & S_IMMUTABLE)
#define IS_APPEND(inode)    ((inode)->flags & S_APPEND)
#define IS_NOATIME(inode)   ((inode)->flags & S_NOATIME)
#define IS_NODUMP(inode)    ((inode)->flags & S_NODUMP)

// dispatch macro: call op or return -ENOTSUP if not implemented
#define VFS_CALL(ops, fn, ...) ((ops)->fn ? (ops)->fn(__VA_ARGS__) : -ENOTSUP)
