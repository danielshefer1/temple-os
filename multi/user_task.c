#include "user_task.h"
#include "scheduler.h"
#include "string.h"
#include "defintions.h"
#include "tty.h"
#include "vfs_file.h"

task_t* create_user_task(const elf64_image_t* img, const char* name) {
    task_t* t = alloc_blank_task(name);
    if (!t) return NULL;

    t->cr3 = img->cr3_phys;

    // Build kstack so first context_switch RET lands in user_task_entry_trampoline,
    // which then IRETQs into ring 3.
    //
    // Layout (low -> high addresses), starting at saved_rsp:
    //   r15=0, r14=0, r13=0, r12=0, rbp=0, rbx=0,
    //   ret = user_task_entry_trampoline,
    //   IRETQ frame: RIP, CS=0x23, RFLAGS=0x202, RSP=user_stack_top, SS=0x1B
    uint64_t* sp = (uint64_t*)t->kstack_top;
    *--sp = 0x1B;                 // SS
    *--sp = img->stack_top;       // RSP (user)
    *--sp = 0x202;                // RFLAGS (IF=1)
    *--sp = 0x23;                 // CS
    *--sp = img->entry;           // RIP
    *--sp = (uint64_t)user_task_entry_trampoline;
    *--sp = 0;                    // rbx
    *--sp = 0;                    // rbp
    *--sp = 0;                    // r12
    *--sp = 0;                    // r13
    *--sp = 0;                    // r14
    *--sp = 0;                    // r15
    t->saved_rsp = (uint64_t)sp;

    // Wire up stdin/stdout/stderr to the global console tty. The three
    // slots each hold a refcounted reference to the same file_t — closing
    // any one of them is independent.
    file_t* tty_f = tty_open(&console_tty);
    if (tty_f) {
        t->fds[0].file = tty_f;
        t->fds[1].file = tty_f; vfs_file_get(tty_f);
        t->fds[2].file = tty_f; vfs_file_get(tty_f);
    }

    // Newest user task becomes foreground for Ctrl+C delivery. Crude but
    // sufficient until job-control / process groups exist.
    console_tty.foreground = t;

    rq_enqueue_external(t);
    return t;
}
