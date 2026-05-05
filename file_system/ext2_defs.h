#pragma once
#include "includes.h"

#define EXT2_S_IFSOCK  0xC000
#define EXT2_S_IFLNK   0xA000
#define EXT2_S_IFREG   0x8000
#define EXT2_S_IFBLK   0x6000
#define EXT2_S_IFDIR   0x4000
#define EXT2_S_IFCHR   0x2000
#define EXT2_S_IFIFO   0x1000

#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7

// i_mode permission bits
#define EXT2_S_ISUID   0x0800       // setuid
#define EXT2_S_ISGID   0x0400       // setgid
#define EXT2_S_ISVTX   0x0200       // sticky
#define EXT2_S_IRUSR   0x0100
#define EXT2_S_IWUSR   0x0080
#define EXT2_S_IXUSR   0x0040
#define EXT2_S_IRGRP   0x0020
#define EXT2_S_IWGRP   0x0010
#define EXT2_S_IXGRP   0x0008
#define EXT2_S_IROTH   0x0004
#define EXT2_S_IWOTH   0x0002
#define EXT2_S_IXOTH   0x0001

// i_flags
#define EXT2_SECRM_FL        0x00000001   // secure deletion
#define EXT2_UNRM_FL         0x00000002   // record for undelete
#define EXT2_COMPR_FL        0x00000004   // compressed file
#define EXT2_SYNC_FL         0x00000008   // synchronous updates
#define EXT2_IMMUTABLE_FL    0x00000010   // immutable file
#define EXT2_APPEND_FL       0x00000020   // append only
#define EXT2_NODUMP_FL       0x00000040   // do not dump
#define EXT2_NOATIME_FL      0x00000080   // do not update atime

#define EXT2_MAGIC              0xEF53
#define EXT2_SUPERBLOCK_OFFSET  1024   // superblock always at byte 1024
#define EXT2_SUPERBLOCK_LENGTH 1024

#define EXT2_VALID_FS    0x0001   // cleanly unmounted
#define EXT2_ERROR_FS    0x0002   // not cleanly unmounted / has errors
#define EXT2_ORPHAN_FS   0x0004   // orphan inodes being recovered

#define EXT2_SYMLINK_PREM 0777
#define MAX_FAST_SYMLINK_LENGTH 60

// inode numbers reserved by ext2 (1-10)
#define EXT2_BAD_INO            1      // bad blocks inode
#define EXT2_ROOT_INO           2      // root directory
#define EXT2_ACL_IDX_INO        3
#define EXT2_ACL_DATA_INO       4
#define EXT2_BOOT_LOADER_INO    5
#define EXT2_UNDEL_DIR_INO      6
#define EXT2_FIRST_INO          11     // first non-reserved inode

#define EXT2_BLOCK_SIZE(sb)         (1024 << (sb)->s_log_block_size)
#define EXT2_BLOCKS_PER_BLOCK(sb)     (sb->block_size / sizeof(uint32_t))
#define EXT2_DIRENT_ALIGN(size) (((size) + 3) & ~3)
