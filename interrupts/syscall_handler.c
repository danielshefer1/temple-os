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
#include "timer.h"
#include "paging.h"
#include "fd_table.h"
#include "vfs_file.h"
#include "pipe.h"

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
        case MKNOD_SYSCALL:        ret = SysMknod(frame);         break;
        case SLEEP_SYSCALL:        ret = SleepHandler(frame);     break;
        case SETPGID_SYSCALL:      ret = SetpgidHandler(frame);   break;
        case GETPGID_SYSCALL:      ret = GetpgidHandler(frame);   break;
        case PIPE_SYSCALL:         ret = PipeHandler(frame);      break;
        case DUP_SYSCALL:          ret = DupHandler(frame);       break;
        case DUP2_SYSCALL:         ret = Dup2Handler(frame);      break;
        case CHDIR_SYSCALL:        ret = SysChdir(frame);         break;
        case GETCWD_SYSCALL:       ret = SysGetcwd(frame);        break;
        case GETDENTS_SYSCALL:     ret = SysGetdents(frame);      break;
        case MMAP_FILE_SYSCALL:    ret = MmapFileHandler(frame);  break;
        case SETSID_SYSCALL:       ret = SetsidHandler(frame);    break;

        default:                   ret = UnknownSysCall();
    }
    frame->rax = (uint64_t) ret;

    // Deliver any pending signal before returning to ring 3. Done here so
    // the saved frame captured by signal_deliver_on_return reflects the
    // syscall's result (rax just set above) and post-syscall RIP — that's
    // what SIGRETURN will restore later.
    signal_deliver_on_return(frame);
}

// Anonymous user mmap: hand back `size` bytes of fresh, zeroed, RW/NX user
// pages mapped at the next free slot in the task's [USER_MMAP_BASE, _END)
// region. Bump-only — munmap holes are not reused. Pages are owned by the
// caller's address space; exit/exec reaps them via free_user_address_space.
int64_t MmapHandler(interrupt_frame_t* frame) {
    uint64_t size = frame->rbx;
    if (size == 0) return -EINVAL;
    size = (size + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1);

    task_t* cur = this_cpu()->current;
    if (cur->mmap_next == 0) cur->mmap_next = USER_MMAP_BASE;
    if (cur->mmap_next + size > USER_MMAP_END) return -ENOMEM;

    uint64_t va_start = cur->mmap_next;
    page_entry_t* pml4_kvirt = (page_entry_t*)(cur->cr3 + KERNEL_VIRTUAL);
    uint64_t flags = PRESENT_PAGE | RW_PAGE | USER_PAGE | NX_PAGE;

    uint64_t mapped = 0;
    for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
        void* phys = RequestBuddy(PAGE_SIZE, false);
        if (!phys) goto fail;
        memset((void*)((uint64_t)phys + KERNEL_VIRTUAL), 0, PAGE_SIZE);
        if (map_page_to_virt_in(pml4_kvirt, va_start + off, (uint64_t)phys, flags, false) < 0) {
            FreeBuddy(phys, false);
            goto fail;
        }
        mapped += PAGE_SIZE;
    }
    cur->mmap_next += size;
    return (int64_t)va_start;

fail:
    for (uint64_t off = 0; off < mapped; off += PAGE_SIZE) {
        uint64_t phys;
        if (lookup_user_in_pml4(cur->cr3, va_start + off, &phys) == 0) {
            unmap_page_in(pml4_kvirt, va_start + off);
            FreeBuddy((void*)phys, false);
        }
    }
    return -ENOMEM;
}

// File-backed mmap. Maps `size` bytes of the file's physical pages into the
// next free slot of the caller's mmap region. Used by /dev/fb so the
// userspace term can splat pixels straight to the framebuffer with no copy.
//
// Driver responsibility (file_ops_t.mmap_phys): given a page-aligned offset,
// return the device's physical page address. This handler walks page-by-page,
// asks the driver for each phys, and maps it user-RW.
int64_t MmapFileHandler(interrupt_frame_t* frame) {
    int64_t  fd   = (int64_t)frame->rbx;
    uint64_t size = frame->rcx;     // arg2: r10 -> rcx by syscall_entry
    if (size == 0) return -EINVAL;
    size = (size + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1);

    file_t* f = fd_lookup(fd);
    if (f == NULL) return -EBADF;
    if (f->ops == NULL || f->ops->mmap_phys == NULL) return -ENOTSUP;

    task_t* cur = this_cpu()->current;
    if (cur->mmap_next == 0) cur->mmap_next = USER_MMAP_BASE;
    if (cur->mmap_next + size > USER_MMAP_END) return -ENOMEM;

    uint64_t va_start = cur->mmap_next;
    page_entry_t* pml4_kvirt = (page_entry_t*)(cur->cr3 + KERNEL_VIRTUAL);
    uint64_t flags = PRESENT_PAGE | RW_PAGE | USER_PAGE | NX_PAGE
                   | WRITE_THROUGH_PAGE | CACHE_DIS_PAGE;

    uint64_t mapped = 0;
    for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
        uint64_t phys = 0;
        int64_t r = f->ops->mmap_phys(f, off, &phys);
        if (r < 0) goto fail;
        if (map_page_to_virt_in(pml4_kvirt, va_start + off, phys, flags, false) < 0) {
            goto fail;
        }
        mapped += PAGE_SIZE;
    }
    cur->mmap_next += size;
    return (int64_t)va_start;

fail:
    // Walk back the partial mapping. We do NOT FreeBuddy these — the pages
    // are owned by the device, not the buddy.
    for (uint64_t off = 0; off < mapped; off += PAGE_SIZE) {
        unmap_page_in(pml4_kvirt, va_start + off);
    }
    return -ENOMEM;
}

// setsid(): make the calling task a new session leader and a new process
// group leader (sid = pgid = pid). Detaches it from its previous controlling
// terminal so a subsequent ioctl(TIOCSCTTY) on a pty slave can attach a
// fresh ctty without inheriting the parent's.
int64_t SetsidHandler(interrupt_frame_t* frame) {
    (void)frame;
    task_t* me = this_cpu()->current;
    me->sid  = me->pid;
    me->pgid = me->pid;
    me->ctty = NULL;
    return (int64_t)me->sid;
}

int64_t MunmapHandler(interrupt_frame_t* frame) {
    uint64_t addr = frame->rbx;
    uint64_t size = frame->rcx;   // arg2: r10 → rcx by syscall_entry
    if (addr & (PAGE_SIZE - 1)) return -EINVAL;
    if (size == 0) return -EINVAL;
    size = (size + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1);
    if (addr < USER_MMAP_BASE || addr + size > USER_MMAP_END) return -EINVAL;

    task_t* cur = this_cpu()->current;
    page_entry_t* pml4_kvirt = (page_entry_t*)(cur->cr3 + KERNEL_VIRTUAL);
    for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
        uint64_t phys;
        if (lookup_user_in_pml4(cur->cr3, addr + off, &phys) < 0) continue;
        unmap_page_in(pml4_kvirt, addr + off);
        FreeBuddy((void*)phys, false);
    }
    return 0;
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
    // POSIX wait-status: clean exits live in bits 8..15 (low byte of the
    // user-supplied code). Signal-death paths in multi/signal.c put the
    // signo in bits 0..6.
    task_exit(((frame->rbx) & 0xFF) << 8);
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
    cur->mmap_next = USER_MMAP_BASE;
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

int64_t SleepHandler(interrupt_frame_t* frame) {
    sleep(frame->rbx);
    return 0;
}

// setpgid(pid, pgid): set the pgid of `pid` (0 = self) to `pgid` (0 = use
// the target's own pid, i.e. start a new pgrp). POSIX restricts this in
// several ways (target must be self or a not-yet-execed child, must be in
// same session, etc.); we only enforce session-equality, since fork is the
// only way to extend a session today and we don't ship setsid.
int64_t SetpgidHandler(interrupt_frame_t* frame) {
    int64_t pid_arg  = (int64_t)frame->rbx;
    int64_t pgid_arg = (int64_t)frame->rcx;
    if (pid_arg < 0 || pgid_arg < 0) return -EINVAL;

    task_t* me = this_cpu()->current;
    task_t* t  = (pid_arg == 0) ? me : task_for_pid((uint64_t)pid_arg);
    if (!t) return -ESRCH;
    if (t->sid != me->sid) return -EPERM;

    uint64_t new_pgid = (pgid_arg == 0) ? t->pid : (uint64_t)pgid_arg;
    t->pgid = new_pgid;
    return 0;
}

// getpgid(pid): return the pgid of `pid` (0 = self).
int64_t GetpgidHandler(interrupt_frame_t* frame) {
    int64_t pid_arg = (int64_t)frame->rbx;
    if (pid_arg < 0) return -EINVAL;

    task_t* t = (pid_arg == 0) ? this_cpu()->current
                               : task_for_pid((uint64_t)pid_arg);
    if (!t) return -ESRCH;
    return (int64_t)t->pgid;
}

int64_t WaitpidHandler(interrupt_frame_t* frame) {
    int64_t   target_pid   = (int64_t)   frame->rbx;
    uint64_t* user_status  = (uint64_t*) frame->rcx;  // patched from r10 by syscall_entry
    return do_waitpid(target_pid, user_status);
}

// dup(fd): allocate the lowest free fd ≥ 3 referencing the same file_t.
int64_t DupHandler(interrupt_frame_t* frame) {
    int64_t fd = (int64_t)frame->rbx;
    file_t* f = fd_lookup(fd);
    if (f == NULL) return -EBADF;
    vfs_file_get(f);
    int64_t newfd = fd_alloc(f);
    if (newfd < 0) {
        vfs_file_put(f);
        return newfd;
    }
    return newfd;
}

// dup2(oldfd, newfd): install oldfd's file_t at newfd, closing whatever was
// there. Returns newfd. No-op (returns newfd) if oldfd == newfd.
int64_t Dup2Handler(interrupt_frame_t* frame) {
    int64_t oldfd = (int64_t)frame->rbx;
    int64_t newfd = (int64_t)frame->rcx;  // arg2: r10 -> rcx by syscall_entry
    if (oldfd < 0 || oldfd >= FD_MAX) return -EBADF;
    if (newfd < 0 || newfd >= FD_MAX) return -EBADF;
    file_t* f = fd_lookup(oldfd);
    if (f == NULL) return -EBADF;
    if (oldfd == newfd) return newfd;
    vfs_file_get(f);
    int64_t r = fd_alloc_at(f, newfd);
    if (r < 0) {
        vfs_file_put(f);
        return r;
    }
    return newfd;
}

// pipe(int fds[2]): allocates one anonymous pipe and two file_t's; installs
// them at the lowest two free fds ≥ 3 and writes {readfd, writefd} to the
// caller's int[2] buffer.
int64_t PipeHandler(interrupt_frame_t* frame) {
    int* user_fds = (int*)frame->rbx;
    if (!user_fds) return -EINVAL;
    if ((uint64_t)user_fds >= 0xFFFF800000000000ULL) return -EINVAL;

    file_t* rf = NULL;
    file_t* wf = NULL;
    int64_t r = pipe_create_pair(&rf, &wf);
    if (r < 0) return r;

    int64_t rfd = fd_alloc(rf);
    if (rfd < 0) { vfs_file_put(rf); vfs_file_put(wf); return rfd; }
    int64_t wfd = fd_alloc(wf);
    if (wfd < 0) {
        // Roll back the read-side install.
        fd_release(rfd);
        vfs_file_put(rf);
        vfs_file_put(wf);
        return wfd;
    }
    user_fds[0] = (int)rfd;
    user_fds[1] = (int)wfd;
    return 0;
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