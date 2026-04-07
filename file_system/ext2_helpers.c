#include "ext2_helpers.h"

uint64_t EXT2ModeToType(uint16_t mode) {
    switch (mode & 0xF000) {
        case EXT2_S_IFREG: return VFS_TYPE_FILE;
        case EXT2_S_IFDIR: return VFS_TYPE_DIR;
        case EXT2_S_IFLNK: return VFS_TYPE_SYMLINK;
        case EXT2_S_IFCHR: return VFS_TYPE_CHARDEV;
        case EXT2_S_IFBLK: return VFS_TYPE_BLOCKDEV;
        case EXT2_S_IFIFO: return VFS_TYPE_FIFO;
        case EXT2_S_IFSOCK: return VFS_TYPE_SOCKET;
        default:     return VFS_TYPE_UNKNOWN;
    }
}

uint32_t EXT2TypeToMode(uint64_t type) {
    switch (type) {
        case VFS_TYPE_FILE: return EXT2_S_IFREG;
        case VFS_TYPE_DIR: return EXT2_S_IFDIR;
        case VFS_TYPE_SYMLINK: return EXT2_S_IFLNK;
        case VFS_TYPE_CHARDEV: return EXT2_S_IFCHR;
        case VFS_TYPE_BLOCKDEV: return EXT2_S_IFBLK;
        case VFS_TYPE_FIFO: return EXT2_S_IFIFO;
        case VFS_TYPE_SOCKET: return EXT2_S_IFSOCK;
        default:     return 0;
    }
}

uint8_t EXT2TypeToFT(uint64_t type) {
    switch (type) {
        case VFS_TYPE_FILE: return EXT2_FT_REG_FILE;
        case VFS_TYPE_DIR: return EXT2_FT_DIR;
        case VFS_TYPE_SYMLINK: return EXT2_FT_SYMLINK;
        case VFS_TYPE_CHARDEV: return EXT2_FT_CHRDEV;
        case VFS_TYPE_BLOCKDEV: return EXT2_FT_BLKDEV;
        case VFS_TYPE_FIFO: return EXT2_FT_FIFO;
        case VFS_TYPE_SOCKET: return EXT2_FT_SOCK;
        default:     return EXT2_FT_UNKNOWN;
    }
}

uint32_t EXT2InodeNumberToGroup(ext2_info_t* vol, uint32_t inode_number) {
    return (inode_number - 1) / vol->inodes_per_group;
}

uint32_t EXT2PremissionsToMod(uint64_t permissions) {
    return permissions & 0xFFF;
}

uint64_t EXT2FlagsToVFSFlags(uint32_t i_flags) {
    uint64_t flags = 0;
    if (i_flags & EXT2_IMMUTABLE_FL) flags |= S_IMMUTABLE;
    if (i_flags & EXT2_APPEND_FL) flags |= S_APPEND;
    if (i_flags & EXT2_NOATIME_FL) flags |= S_NOATIME;
    if (i_flags & EXT2_NODUMP_FL) flags |= S_NODUMP;
    if (i_flags & EXT2_SYNC_FL) flags |= S_SYNC;

    return flags;
}

uint32_t EXT2VFSFlagsToIFlags(uint64_t flags) {
    uint32_t i_flags = 0;
    if (flags & S_IMMUTABLE) i_flags |= EXT2_IMMUTABLE_FL;
    if (flags & S_APPEND) i_flags |= EXT2_APPEND_FL;
    if (flags & S_NOATIME) i_flags |= EXT2_NOATIME_FL;
    if (flags & S_NODUMP) i_flags |= EXT2_NODUMP_FL;
    if (flags & S_SYNC) i_flags |= EXT2_SYNC_FL;

    return i_flags;
}