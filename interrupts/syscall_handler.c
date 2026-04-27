#include "syscall_handler.h"

void syscall_handler(interrupt_frame_t* frame) {
    uint64_t cs = frame->cs;
    if (cs == 0x08) return;

    uint64_t syscall_id = frame->rax;
    int64_t  ret;

    switch (syscall_id) {
        case EXIT_SYSCALL:         ret = ExitHandler();           break;
        case FLUSH_BUFFER_SYSCALL: ret = FlushBufferHandler();    break;
        case MMAP_SYSCALL:         ret = MmapHandler(frame);      break;
        case MUNMAP_SYSCALL:       ret = MunmapHandler(frame);    break;

        case OPEN_SYSCALL:         ret = SysOpen(frame);          break;
        case CLOSE_SYSCALL:        ret = SysClose(frame);         break;
        case FREAD_SYSCALL:        ret = SysFRead(frame);         break;
        case FWRITE_SYSCALL:       ret = SysFWrite(frame);        break;
        case LSEEK_SYSCALL:        ret = SysLseek(frame);         break;
        case TRUNCATE_SYSCALL:     ret = SysTruncate(frame);      break;
        case UNLINK_SYSCALL:       ret = SysUnlink(frame);        break;
        case MKDIR_SYSCALL:        ret = SysMkdir(frame);         break;
        case RMDIR_SYSCALL:        ret = SysRmdir(frame);         break;
        case RENAME_SYSCALL:       ret = SysRename(frame);        break;
        case SYMLINK_SYSCALL:      ret = SysSymlink(frame);       break;
        case READLINK_SYSCALL:     ret = SysReadlink(frame);      break;
        case STAT_SYSCALL:         ret = SysStat(frame);          break;
        case SYNC_SYSCALL:         ret = SysSync(frame);          break;
        case IOCTL_SYSCALL:        ret = SysIoctl(frame);         break;

        default:                   ret = UnknownSysCall();
    }
    frame->rax = (uint64_t) ret;
}

int64_t MmapHandler(interrupt_frame_t* frame) {
    int64_t size = frame->rbx;
    void* ret = RequestBuddy(size, true);
    return (int64_t) ret;
}

int64_t MunmapHandler(interrupt_frame_t* frame) {
    int64_t addr = frame->rbx;
    FreeBuddy((void*) addr, true);
    return 1;
}

int64_t UnknownSysCall() {
    kprintf("This is not a known SYSCALL");
    return -1;
}

int64_t FlushBufferHandler() {
    FlushBuffer(&console_buffer);
    return 1;
}

int64_t ExitHandler() {
    CliHelper();
    HltHelper();
    return 1;
}