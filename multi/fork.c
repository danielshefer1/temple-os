#include "fork.h"
#include "scheduler.h"
#include "cpu_local.h"
#include "pml4_clone.h"
#include "string.h"
#include "defintions.h"
#include "vfs_file.h"

// Implemented inside interrupts/syscall_entry.asm — points at the POPAQ +
// SYSRET tail of the syscall return path, immediately after the
// `call syscall_handler` site. The child's kstack is synthesized so
// context_switch's final RET lands here.
extern void fork_child_return(void);

int64_t do_fork(interrupt_frame_t* parent_frame) {
    cpu_local_t* cpu = this_cpu();
    task_t* parent = cpu->current;

    uint64_t child_cr3 = 0;
    int64_t r = clone_user_pml4(parent->cr3, &child_cr3);
    if (r < 0) return r;

    task_t* child = alloc_blank_task(parent->name);
    if (!child) {
        free_user_address_space(child_cr3);
        free_cloned_pml4(child_cr3);
        return -ENOMEM;
    }
    child->cr3 = child_cr3;
    child->parent = parent;

    // Duplicate the parent's open-file table. Both tasks now reference the
    // same file_t structs (so they share file offsets, matching POSIX),
    // but each has its own fd-number space — closing in one doesn't affect
    // the other. Bump the refcount once per non-NULL slot so file_t
    // teardown waits until both tasks have closed.
    for (int64_t i = 0; i < FD_MAX; i++) {
        child->fds[i] = parent->fds[i];
        if (child->fds[i].file != NULL) {
            vfs_file_get(child->fds[i].file);
        }
    }

    // Synthesize the child's kernel stack. Layout from high to low addresses:
    //
    //   [interrupt_frame_t]    <- copy of parent_frame, but rax = 0 and the
    //                             rsp slot (struct gap) is a zeroed placeholder
    //   [ret_addr]             <- fork_child_return: where context_switch's
    //                             RET will jump after restoring callee-saves
    //   [r15][r14][r13][r12][rbp][rbx]   <- popped by context_switch (in this
    //                                       order from low->high addresses)
    //   <- saved_rsp
    //
    // When the scheduler eventually picks this child:
    //   1. context_switch loads child->cr3 (own user PML4) and rsp = saved_rsp.
    //   2. It pops the six callee-saves, then RETs into fork_child_return.
    //   3. fork_child_return is the existing post-handler tail of
    //      syscall_entry.asm: skip gs/fs, POPAQ (rax=0 for the child), drop
    //      int_no/err_code, then SYSRET into ring 3 at the parent's RIP.
    //
    // POPAQ's order in syscall_entry.asm (low->high): r15..r8, rdi, rsi, rbp,
    // rsp_slot, rbx, rdx, rcx, rax. We push from high (ss) downward so the
    // bytes match what POPAQ expects.

    uint64_t* sp = (uint64_t*)child->kstack_top;

    // ---- interrupt_frame_t copy with rax forced to 0 ----
    *--sp = parent_frame->ss;
    *--sp = parent_frame->userrsp;
    *--sp = parent_frame->qflags;
    *--sp = parent_frame->cs;
    *--sp = parent_frame->rip;
    *--sp = parent_frame->err_code;
    *--sp = parent_frame->int_no;
    *--sp = 0;                              // rax = 0 in child
    *--sp = parent_frame->rcx;
    *--sp = parent_frame->rdx;
    *--sp = parent_frame->rbx;
    *--sp = 0;                              // struct rsp gap
    *--sp = parent_frame->rbp;
    *--sp = parent_frame->rsi;
    *--sp = parent_frame->rdi;
    *--sp = parent_frame->r8;
    *--sp = parent_frame->r9;
    *--sp = parent_frame->r10;
    *--sp = parent_frame->r11;
    *--sp = parent_frame->r12;
    *--sp = parent_frame->r13;
    *--sp = parent_frame->r14;
    *--sp = parent_frame->r15;
    *--sp = parent_frame->fs;
    *--sp = parent_frame->gs;

    // ---- context_switch RET target ----
    *--sp = (uint64_t)fork_child_return;

    // ---- Six zeroed callee-saves popped by context_switch in the order
    //      r15, r14, r13, r12, rbp, rbx (low->high). Pushing rbx first since
    //      sp grows down means rbx ends up at the highest address. ----
    *--sp = 0;                              // rbx
    *--sp = 0;                              // rbp
    *--sp = 0;                              // r12
    *--sp = 0;                              // r13
    *--sp = 0;                              // r14
    *--sp = 0;                              // r15

    child->saved_rsp = (uint64_t)sp;

    rq_enqueue_external(child);
    return (int64_t)child->pid;
}
