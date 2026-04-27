#include "vfs_syscalls.h"

// TODO: when user paging exists, replace direct pointer use with copy_from_user.

int64_t SysOpen(interrupt_frame_t* f) {
    const char* path = (const char*) f->rbx;
    uint32_t flags   = (uint32_t)    f->rcx;
    uint64_t mode    = (uint64_t)    f->rdx;

    if (path == NULL) return -EINVAL;

    file_t* fp = NULL;
    int64_t r = vfs_open_path(path, flags, mode, &fp);
    if (r < 0) return r;

    int64_t fd = fd_alloc(fp);
    if (fd < 0) {
        vfs_file_put(fp);
        return fd;
    }
    return fd;
}

int64_t SysClose(interrupt_frame_t* f) {
    int64_t fd = (int64_t) f->rbx;
    if (fd >= STDIN_FILENO && fd <= STDERR_FILENO) return 0;
    file_t* fp = fd_release(fd);
    if (fp == NULL) return -EBADF;
    vfs_file_put(fp);
    return 0;
}

int64_t SysFRead(interrupt_frame_t* f) {
    int64_t  fd   = (int64_t) f->rbx;
    void*    buf  = (void*)   f->rcx;
    uint64_t size = (uint64_t)f->rdx;

    if (fd == STDIN_FILENO) {
        if (buf == NULL && size > 0) return -EINVAL;
        tuple_t newline_triggers = { '\n', '\n' };
        return (int64_t) GetInputUntilKey(&console_buffer, (char*)buf, size,
                                          KEYBOARD_MS_BACK, &newline_triggers);
    }

    file_t* fp = fd_lookup(fd);
    if (fp == NULL) return -EBADF;
    if (buf == NULL && size > 0) return -EINVAL;
    return vfs_read(fp, buf, size);
}

int64_t SysFWrite(interrupt_frame_t* f) {
    int64_t      fd   = (int64_t)    f->rbx;
    const void*  buf  = (const void*)f->rcx;
    uint64_t     size = (uint64_t)   f->rdx;

    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        if (buf == NULL && size > 0) return -EINVAL;
        return (int64_t) print_str_SYSCALL((const char*)buf, GREY_COLOR, size);
    }

    file_t* fp = fd_lookup(fd);
    if (fp == NULL) return -EBADF;
    if (buf == NULL && size > 0) return -EINVAL;
    return vfs_write(fp, buf, size);
}

int64_t SysLseek(interrupt_frame_t* f) {
    int64_t fd     = (int64_t) f->rbx;
    int64_t offset = (int64_t) f->rcx;
    int64_t whence = (int64_t) f->rdx;

    file_t* fp = fd_lookup(fd);
    if (fp == NULL) return -EBADF;
    return vfs_seek(fp, offset, whence);
}

int64_t SysTruncate(interrupt_frame_t* f) {
    int64_t  fd       = (int64_t)  f->rbx;
    uint64_t new_size = (uint64_t) f->rcx;

    file_t* fp = fd_lookup(fd);
    if (fp == NULL) return -EBADF;
    return vfs_truncate(fp, new_size);
}

int64_t SysUnlink(interrupt_frame_t* f) {
    const char* path = (const char*) f->rbx;
    if (path == NULL) return -EINVAL;
    return vfs_unlink_path(path);
}

int64_t SysMkdir(interrupt_frame_t* f) {
    const char* path = (const char*) f->rbx;
    uint64_t    perm = (uint64_t)    f->rcx;
    if (path == NULL) return -EINVAL;
    return vfs_mkdir_path(path, perm);
}

int64_t SysRmdir(interrupt_frame_t* f) {
    const char* path = (const char*) f->rbx;
    if (path == NULL) return -EINVAL;
    return vfs_rmdir_path(path);
}

int64_t SysRename(interrupt_frame_t* f) {
    const char* old_path = (const char*) f->rbx;
    const char* new_path = (const char*) f->rcx;
    if (old_path == NULL || new_path == NULL) return -EINVAL;
    return vfs_rename_path(old_path, new_path);
}

int64_t SysSymlink(interrupt_frame_t* f) {
    const char* target   = (const char*) f->rbx;
    const char* linkpath = (const char*) f->rcx;
    if (target == NULL || linkpath == NULL) return -EINVAL;
    return vfs_symlink_path(target, linkpath);
}

int64_t SysReadlink(interrupt_frame_t* f) {
    const char* path = (const char*) f->rbx;
    char*       buf  = (char*)       f->rcx;
    uint64_t    size = (uint64_t)    f->rdx;
    if (path == NULL || buf == NULL || size == 0) return -EINVAL;
    return vfs_readlink_path(path, buf, size);
}

int64_t SysStat(interrupt_frame_t* f) {
    const char* path = (const char*) f->rbx;
    fs_stat_t*  out  = (fs_stat_t*)  f->rcx;
    if (path == NULL || out == NULL) return -EINVAL;
    return vfs_stat_path(path, out);
}

int64_t SysSync(interrupt_frame_t* f) {
    (void) f;
    if (vfs_root == NULL || vfs_root->inode == NULL) return -EINVAL;
    return vfs_sync(vfs_root->inode->sb);
}

int64_t SysIoctl(interrupt_frame_t* f) {
    int64_t  fd  = (int64_t)  f->rbx;
    uint64_t cmd = (uint64_t) f->rcx;
    void*    arg = (void*)    f->rdx;

    file_t* fp = fd_lookup(fd);
    if (fp == NULL) return -EBADF;
    return vfs_ioctl(fp, cmd, arg);
}
