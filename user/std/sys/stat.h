#pragma once

// Per-inode metadata returned by sys_stat. Layout must match the kernel's
// fs_inode_stat_t in file_system/vfs_types.h byte-for-byte.

typedef struct stat {
    unsigned long st_type;       // VFS_TYPE_* mirror (S_IF* below)
    unsigned long st_mode;       // permission bits (low 12)
    unsigned long st_size;
    unsigned long st_nlinks;
    unsigned int  st_uid;
    unsigned int  st_gid;
    unsigned int  st_dev_id;     // for char/block dev
    unsigned int  reserved_;
    unsigned long st_atime;      // seconds since epoch
    unsigned long st_mtime;
    unsigned long st_ctime;
} stat_t;

// File-type constants — values mirror VFS_TYPE_* in
// file_system/vfs_defs.h. Both the legacy *_T names (used by sys_mknod)
// and the conventional S_IFREG/S_IFDIR/... spellings live here.

#define S_IFREG    0x01
#define S_IFDIR    0x02
#define S_IFLNK    0x03
#define S_IFCHR    0x04
#define S_IFBLK    0x05
#define S_IFIFO    0x06
#define S_IFSOCK   0x07

#define S_IFCHR_T  S_IFCHR
#define S_IFBLK_T  S_IFBLK
#define S_IFIFO_T  S_IFIFO

// POSIX-style type predicates over a stat_t.st_type or VFS type code.
#define S_ISREG(t)   ((t) == S_IFREG)
#define S_ISDIR(t)   ((t) == S_IFDIR)
#define S_ISLNK(t)   ((t) == S_IFLNK)
#define S_ISCHR(t)   ((t) == S_IFCHR)
#define S_ISBLK(t)   ((t) == S_IFBLK)
#define S_ISFIFO(t)  ((t) == S_IFIFO)
#define S_ISSOCK(t)  ((t) == S_IFSOCK)

// dev_id encoding (Linux old form): low byte = minor, next byte = major.
#define UMKDEV(maj, min) ((((unsigned)(maj) & 0xFFu) << 8) | ((unsigned)(min) & 0xFFu))
