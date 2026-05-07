#include "syscall_handler.h"
#include "elf64.h"
#include "user_task.h"
#include "scheduler.h"
#include "cpu_local.h"
#include "pml4_clone.h"
#include "slab_alloc.h"
#include "string.h"
#include "fork.h"
#include "signal.h"
#include "waitpid.h"

void syscall_handler(interrupt_frame_t* frame) {
    uint64_t cs = frame->cs;
    if (cs == 0x08) return;

    uint64_t syscall_id = frame->rax;
    int64_t  ret;

    switch (syscall_id) {
        case EXIT_SYSCALL:         ret = ExitHandler(frame);      break;
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
        case EXEC_SYSCALL:         ret = ExecHandler(frame);      break;
        case SPAWN_SYSCALL:        ret = SpawnHandler(frame);     break;
        case FORK_SYSCALL:         ret = ForkHandler(frame);      break;
        case KILL_SYSCALL:         ret = KillHandler(frame);      break;
        case SIGNAL_SYSCALL:       ret = SignalHandler(frame);    break;
        case SIGRETURN_SYSCALL:    ret = SigreturnHandler(frame); break;
        case GETPID_SYSCALL:       ret = GetpidHandler(frame);    break;
        case WAITPID_SYSCALL:      ret = WaitpidHandler(frame);   break;

        default:                   ret = UnknownSysCall();
    }
    frame->rax = (uint64_t) ret;

    // Deliver any pending signal before returning to ring 3. Done here so
    // the saved frame captured by signal_deliver_on_return reflects the
    // syscall's result (rax just set above) and post-syscall RIP — that's
    // what SIGRETURN will restore later.
    signal_deliver_on_return(frame);
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
    // Legacy syscall: previously cleared the unsynchronized global keyboard
    // ring. With per-task tty fds the ring lives inside the tty and is
    // drained by tty_read; nothing to flush here. Kept as a no-op so the
    // syscall number stays stable.
    return 0;
}

int64_t ExitHandler(interrupt_frame_t* frame) {
    task_exit(frame->rbx);
    return 1; // unreachable
}

// POSIX-style execve: replaces the calling task's user image in place.
// Same PID, same parent, same fd table; only the address space and the
// IRETQ register frame change. Does not return on success — control
// returns to userspace at the new program's entry point.
int64_t ExecHandler(interrupt_frame_t* frame) {
    const char* upath = (const char*)frame->rbx;
    if (!upath) return -EINVAL;
    // Reject obviously bad pointers (kernel half).
    if ((uint64_t)upath >= 0xFFFF800000000000ULL) return -EINVAL;

    // Copy the path into a kernel buffer before we tear down the user
    // address space — `upath` lives in the soon-to-be-freed image.
    char* path = (char*)kmalloc(EXEC_PATH_MAX);
    if (!path) return -ENOMEM;
    uint64_t i;
    for (i = 0; i < EXEC_PATH_MAX - 1; i++) {
        path[i] = upath[i];
        if (path[i] == 0) break;
    }
    if (i == EXEC_PATH_MAX - 1) { kfree(path, EXEC_PATH_MAX); return -ENAMETOOLONG; }
    path[i] = 0;

    // Build the new image in a fresh PML4. If load_elf64 fails the old
    // address space is untouched and we return -errno cleanly.
    elf64_image_t img;
    int64_t r = load_elf64(path, &img);
    if (r < 0) { kfree(path, EXEC_PATH_MAX); return r; }

    // Commit. Past this point execve cannot fail.
    task_t* cur = this_cpu()->current;
    uint64_t old_cr3 = cur->cr3;

    cur->cr3 = img.cr3_phys;
    switch_pml4((page_entry_t*)img.cr3_phys);

    free_user_address_space(old_cr3);
    free_cloned_pml4(old_cr3);

    // Take on the new program's name (truncate to the task name buffer).
    uint64_t namelen = sizeof(cur->name) - 1;
    for (uint64_t j = 0; j <= namelen; j++) {
        cur->name[j] = (j < namelen) ? path[j] : 0;
        if (cur->name[j] == 0) break;
    }

    kfree(path, EXEC_PATH_MAX);

    // Rewrite the IRETQ portion of the syscall's interrupt frame so that
    // when this handler returns, control lands in ring 3 at the new entry.
    frame->rip     = img.entry;
    frame->cs      = 0x23;          // user code segment
    frame->qflags  = 0x202;         // IF=1, reserved bit
    frame->userrsp = img.stack_top; // SysV initial frame from setup_user_stack
    frame->ss      = 0x1B;          // user data segment

    // Clear GPRs. SysV requires RDX=0 at entry; the rest are unspecified
    // but zeroing avoids leaking kernel state into the new program.
    frame->rax = frame->rbx = frame->rcx = frame->rdx = 0;
    frame->rsi = frame->rdi = frame->rbp = 0;
    frame->r8  = frame->r9  = frame->r10 = frame->r11 = 0;
    frame->r12 = frame->r13 = frame->r14 = frame->r15 = 0;

    return 0;
}

// POSIX fork. The child returns 0; the parent gets the new PID. See
// multi/fork.c for the kstack-synthesis trick that lets the child resume
// in user mode through the existing syscall return path with rax=0.
int64_t ForkHandler(interrupt_frame_t* frame) {
    return do_fork(frame);
}

int64_t KillHandler(interrupt_frame_t* frame) {
    int64_t  pid   = (int64_t) frame->rbx;
    int64_t  signo = (int64_t) frame->rcx;
    if (pid <= 0) return -EINVAL;
    task_t* t = task_for_pid((uint64_t)pid);
    if (!t) return -ESRCH;
    signal_send(t, (int)signo);
    return 0;
}

int64_t SignalHandler(interrupt_frame_t* frame) {
    int64_t signo    = (int64_t) frame->rbx;
    void*   handler  = (void*)   frame->rcx;
    void*   restorer = (void*)   frame->rdx;
    return signal_install((int)signo, handler, restorer);
}

int64_t SigreturnHandler(interrupt_frame_t* frame) {
    return signal_sigreturn(frame);
}

int64_t GetpidHandler(interrupt_frame_t* frame) {
    (void)frame;
    return (int64_t) this_cpu()->current->pid;
}

int64_t WaitpidHandler(interrupt_frame_t* frame) {
    int64_t   target_pid   = (int64_t)   frame->rbx;
    uint64_t* user_status  = (uint64_t*) frame->rcx;  // patched from r10 by syscall_entry
    return do_waitpid(target_pid, user_status);
}

// posix_spawn-style: load an ELF from `path` into a fresh user address
// space and create a brand-new task running it. The caller continues to
// run unchanged; the return value is the new task's PID.
int64_t SpawnHandler(interrupt_frame_t* frame) {
    const char* path = (const char*)frame->rbx;
    if (!path) return -EINVAL;
    if ((uint64_t)path >= 0xFFFF800000000000ULL) return -EINVAL;

    elf64_image_t img;
    int64_t r = load_elf64(path, &img);
    if (r < 0) return r;

    task_t* t = create_user_task(&img, path);
    if (!t) return -ENOMEM;
    return (int64_t)t->pid;
}