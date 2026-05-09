#include "scheduler.h"
#include "cpu_local.h"
#include "paging.h"
#include "memory.h"
#include "string.h"
#include "vga.h"
#include "extern.h"
#include "global.h"
#include "defintions.h"
#include "slab_alloc.h"
#include "vfs_file.h"
#include "pml4_clone.h"
#include "tty.h"
#include <stddef.h>

// ---- Global zombie list -------------------------------------------------
// Tasks that have called task_exit but haven't been claimed by waitpid live
// here, keyed by their parent pointer. Orphan tasks (parent == NULL) are
// freed immediately by drain_pending_reap and never enter this list.
static task_t* zombie_list_head = NULL;
static spinlock_t zombie_list_lock = {0};

static void zombie_list_add(task_t* t) {
    spin_lock(&zombie_list_lock);
    t->zombie_next = zombie_list_head;
    zombie_list_head = t;
    spin_unlock(&zombie_list_lock);
}

task_t* zombie_list_take(task_t* parent, uint64_t target_pid) {
    spin_lock(&zombie_list_lock);
    task_t** link = &zombie_list_head;
    task_t* t = zombie_list_head;
    while (t) {
        int parent_ok = (t->parent == parent);
        int pid_ok    = (target_pid == 0) || (t->pid == target_pid);
        if (parent_ok && pid_ok) {
            *link = t->zombie_next;
            t->zombie_next = NULL;
            spin_unlock(&zombie_list_lock);
            return t;
        }
        link = &t->zombie_next;
        t = t->zombie_next;
    }
    spin_unlock(&zombie_list_lock);
    return NULL;
}

#define DEFAULT_TIME_SLICE 20   // ticks (1 ms each) — 20 ms quantum

_Static_assert(offsetof(task_t, fxstate) == TASK_OFF_FXSTATE,
               "TASK_OFF_FXSTATE out of sync with task_t layout");

static uint8_t default_fxstate[512] __attribute__((aligned(16)));

void fpu_init_template(void) {
    __asm__ volatile("fninit; fxsave64 %0" : "=m"(default_fxstate));
}

static inline run_queue_t* rq_for_this_cpu(void) {
    return &this_cpu()->rq;
}

// Hook: which queue should a (newly-ready or re-enqueued) task land on?
// Each task is pinned to its home CPU; we never migrate.
static inline run_queue_t* rq_for_task(task_t* t) {
    return &cpu_locals[t->home_cpu].rq;
}

static uint64_t next_pid = 1;
static spinlock_t pid_lock = {0};
static volatile uint64_t next_assign_cpu = 0;

static uint64_t alloc_pid(void) {
    spin_lock(&pid_lock);
    uint64_t p = next_pid++;
    spin_unlock(&pid_lock);
    return p;
}

static void rq_init(run_queue_t* rq) {
    rq->head = rq->tail = NULL;
    rq->lock.locked = 0;
}

static void rq_enqueue_locked(run_queue_t* rq, task_t* t) {
    t->next = NULL;
    t->prev = rq->tail;
    if (rq->tail) rq->tail->next = t;
    else          rq->head = t;
    rq->tail = t;
}

static task_t* rq_dequeue_locked(run_queue_t* rq) {
    task_t* t = rq->head;
    if (!t) return NULL;
    rq->head = t->next;
    if (rq->head) rq->head->prev = NULL;
    else          rq->tail = NULL;
    t->next = t->prev = NULL;
    return t;
}

static void rq_enqueue(run_queue_t* rq, task_t* t) {
    spin_lock(&rq->lock);
    rq_enqueue_locked(rq, t);
    spin_unlock(&rq->lock);
}

void rq_enqueue_external(task_t* t) {
    rq_enqueue(rq_for_task(t), t);
}

task_t* task_for_pid(uint64_t pid) {
    uint64_t online = cpus_active ? cpus_active : 1;
    for (uint64_t i = 0; i < online; i++) {
        cpu_local_t* cpu = &cpu_locals[i];
        task_t* cur = cpu->current;
        if (cur && cur->pid == pid) return cur;

        run_queue_t* rq = &cpu->rq;
        spin_lock(&rq->lock);
        for (task_t* t = rq->head; t; t = t->next) {
            if (t->pid == pid) {
                spin_unlock(&rq->lock);
                return t;
            }
        }
        spin_unlock(&rq->lock);
    }
    return NULL;
}

void SchedulerInit(void) {
    for (uint32_t i = 0; i < MAX_CPUS; i++) {
        rq_init(&cpu_locals[i].rq);
        cpu_locals[i].sleep_head = NULL;
        cpu_locals[i].sleep_lock.locked = 0;
    }
    pid_lock.locked = 0;
    next_pid = 1;
    next_assign_cpu = 0;
}

void sleep_queue_insert(cpu_local_t* cpu, task_t* t) {
    spin_lock(&cpu->sleep_lock);
    task_t** link = &cpu->sleep_head;
    while (*link && (*link)->sleep_deadline <= t->sleep_deadline) {
        link = &(*link)->sleep_next;
    }
    t->sleep_next = *link;
    *link = t;
    spin_unlock(&cpu->sleep_lock);
}

void sleep_queue_wake_expired(cpu_local_t* cpu) {
    uint64_t now = timer_ticks[cpu->cpu_index];
    spin_lock(&cpu->sleep_lock);
    while (cpu->sleep_head && cpu->sleep_head->sleep_deadline <= now) {
        task_t* t = cpu->sleep_head;
        cpu->sleep_head = t->sleep_next;
        t->sleep_next = NULL;
        t->sleep_deadline = 0;
        t->state = TASK_STATE_READY;
        rq_enqueue_external(t);
    }
    spin_unlock(&cpu->sleep_lock);
}

task_t* alloc_blank_task(const char* name) {
    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    memset(t, 0, sizeof(task_t));

    uint64_t stack_base = AddStack();
    uint64_t stack_top  = stack_base + STACK_PAGES * PAGE_SIZE;

    t->pid          = alloc_pid();
    t->pgid         = t->pid;   // session leader by default; fork() / setpgid override
    t->sid          = t->pid;
    t->state        = TASK_STATE_READY;
    t->cr3          = KERNEL_VIRT_TO_PHYS(PageDirAddrV());
    t->kstack_base  = stack_base;
    t->kstack_top   = stack_top;
    t->kstack_pages = STACK_PAGES;
    t->time_slice   = DEFAULT_TIME_SLICE;

    uint64_t online = cpus_active ? cpus_active : 1;
    t->home_cpu = (uint32_t)(__atomic_fetch_add(&next_assign_cpu, 1, __ATOMIC_RELAXED) % online);

    if (name) {
        for (uint64_t i = 0; i < 31 && name[i]; i++) t->name[i] = name[i];
    }

    // cwd defaults to the global root. Tasks created before vfs_mount_root
    // (e.g. the BSP bootstrap task) get NULL here, which vfs_namei treats as
    // "fall back to vfs_root". fork() overrides this with the parent's cwd.
    t->cwd = vfs_root;

    memcpy(t->fxstate, default_fxstate, sizeof(t->fxstate));
    return t;
}

task_t* create_kernel_task(void (*entry)(void), const char* name) {
    task_t* t = alloc_blank_task(name);
    uint64_t stack_top = t->kstack_top;

    // Build the fake initial stack frame that context_switch will pop.
    // Layout (low -> high addresses) at saved_rsp:
    //   r15=0, r14=0, r13=0, r12=entry, rbp=0, rbx=0, ret=task_entry_trampoline
    uint64_t* sp = (uint64_t*)stack_top;
    *--sp = (uint64_t)task_entry_trampoline;  // return address for context_switch's RET
    *--sp = 0;                                // rbx
    *--sp = 0;                                // rbp
    *--sp = (uint64_t)entry;                  // r12 — trampoline calls this
    *--sp = 0;                                // r13
    *--sp = 0;                                // r14
    *--sp = 0;                                // r15
    t->saved_rsp = (uint64_t)sp;

    rq_enqueue(rq_for_task(t), t);
    return t;
}

void scheduler_attach_bootstrap(const char* name) {
    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    memset(t, 0, sizeof(task_t));
    t->pid          = alloc_pid();
    t->state        = TASK_STATE_RUNNING;
    t->cr3          = KERNEL_VIRT_TO_PHYS(PageDirAddrV());
    t->kstack_top   = this_cpu()->kstack_top;
    t->kstack_base  = 0;             // owned by BSP/AP startup, not freeable
    t->kstack_pages = 0;
    t->time_slice   = DEFAULT_TIME_SLICE;
    t->home_cpu     = this_cpu()->cpu_index;
    if (name) {
        for (int i = 0; i < 31 && name[i]; i++) t->name[i] = name[i];
    }
    this_cpu()->current = t;
}

void free_dead_task(task_t* dead) {
    if (!dead) return;

    // If this task was the tty's foreground (Ctrl+C target), clear the
    // pointer so signal_send doesn't dereference freed memory next time
    // someone hits Ctrl+C.
    tty_drop_task(dead);

    // Close any fds the dying task still held open. Refcount decrement may
    // free the underlying file_t when this was the last referencer.
    for (int64_t i = 0; i < FD_MAX; i++) {
        if (dead->fds[i].file != NULL) {
            vfs_file_put(dead->fds[i].file);
            dead->fds[i].file = NULL;
        }
    }

    // Free the user address space if this task had its own PML4. Kernel
    // tasks share the global pml4 (cr3 == phys of pml4) — never free that.
    uint64_t global_cr3 = KERNEL_VIRT_TO_PHYS(PageDirAddrV());
    if (dead->cr3 != global_cr3) {
        free_user_address_space(dead->cr3);
        free_cloned_pml4(dead->cr3);
    }

    if (dead->kstack_pages) {
        RemoveKernelPages(dead->kstack_base, dead->kstack_pages);
    }
    kfree(dead, sizeof(task_t));
}

// Free a zombie left behind by a previous context_switch on this CPU. Safe
// to call now because context_switch has long since moved off the zombie's
// stack — we're running on some other task's stack at this point.
//
// If the zombie has a live parent, it is held on the global zombie list by
// task_exit so the parent's waitpid can claim it; this function only fully
// reaps tasks that have no live waiter — kernel tasks (parent == NULL),
// orphans whose parent already died, etc.
static void drain_pending_reap(cpu_local_t* cpu) {
    task_t* dead = cpu->pending_reap;
    if (!dead) return;
    cpu->pending_reap = NULL;

    // If a parent is still alive, leave the corpse on the zombie list for
    // waitpid. task_exit already enqueued it.
    if (dead->parent != NULL && dead->parent->state != TASK_STATE_ZOMBIE) {
        return;
    }
    free_dead_task(dead);
}

void schedule(void) {
    bool ie = check_interrupts();
    CliHelper();

    cpu_local_t* cpu = this_cpu();
    drain_pending_reap(cpu);

    task_t* prev = cpu->current;
    run_queue_t* rq = rq_for_this_cpu();

    spin_lock(&rq->lock);
    task_t* next = rq_dequeue_locked(rq);
    if (!next) {
        // Nothing to run. If prev still wants the CPU (RUNNING), keep it.
        // But if prev parked itself (BLOCKED / ZOMBIE), we MUST NOT return:
        // the caller (e.g. mutex_lock) is relying on us not coming back
        // until a wakeup puts something on the queue. Idle here with
        // interrupts on so the wakeup IPI / timer can land.
        if (prev && prev->state != TASK_STATE_RUNNING) {
            spin_unlock(&rq->lock);
            kprintf("<idle-loop cpu=%d state=%d>", (uint64_t)cpu->cpu_index, (uint64_t)prev->state);
            while (true) {
                StiHelper();
                HltHelper();
                CliHelper();
                spin_lock(&rq->lock);
                next = rq_dequeue_locked(rq);
                if (next) break;
                // A wakeup may have flipped prev back to READY/RUNNING
                // without going through the queue (shouldn't happen, but
                // defensive); honor it and bail.
                if (prev->state == TASK_STATE_RUNNING) {
                    spin_unlock(&rq->lock);
                    if (ie) StiHelper();
                    return;
                }
                spin_unlock(&rq->lock);
            }
            // fall through with rq still locked and next != NULL
        } else {
            spin_unlock(&rq->lock);
            if (ie) StiHelper();
            return;
        }
    }
    // BLOCKED tasks (e.g. parked by mutex_lock) intentionally fall through:
    // they are not re-enqueued here. The corresponding mutex_unlock /
    // wakeup path is responsible for putting them back on the run queue.
    if (prev && prev->state == TASK_STATE_RUNNING) {
        prev->state = TASK_STATE_READY;
        // Re-enqueue on the queue this task should land on. With a single
        // global queue this is the same as rq, but per-CPU schedulers may
        // pin prev to its home CPU.
        run_queue_t* prev_rq = rq_for_task(prev);
        if (prev_rq == rq) {
            rq_enqueue_locked(rq, prev);
        } else {
            spin_unlock(&rq->lock);
            rq_enqueue(prev_rq, prev);
            goto picked;
        }
    }
    spin_unlock(&rq->lock);
picked:;

    next->state = TASK_STATE_RUNNING;
    next->time_slice = DEFAULT_TIME_SLICE;

    // Per-CPU updates in C — kernel-mode GS base is 0 on this OS so the asm
    // can't touch gs:[]. Update before the swap so the new task lands with
    // its TSS/current pointers already correct on this CPU.
    cpu->current = next;
    cpu->kernel_rsp = next->kstack_top;
    if (cpu->tss) cpu->tss->rsp0 = next->kstack_top;

    // If prev is exiting, hand it to this CPU's reap slot. The next
    // schedule() call on whichever CPU resumes will free it — by then
    // context_switch has already moved off prev's kernel stack.
    //
    // Only orphans / kernel tasks (parent == NULL) take this path. A zombie
    // with a live parent has already been linked into the global zombie
    // list by task_exit; the parent's waitpid is responsible for freeing
    // it. Putting it on pending_reap as well would race waitpid: drain
    // would later deref a task_t that waitpid had already kfreed.
    if (prev && prev->state == TASK_STATE_ZOMBIE && prev->parent == NULL) {
        cpu->pending_reap = prev;
    }

    if (prev != next) context_switch(prev, next);

    // Resumed: restore IF if it was on at the call site.
    if (ie) StiHelper();
}

void task_exit(uint64_t exit_code) {
    CliHelper();
    cpu_local_t* cpu = this_cpu();
    task_t* cur = cpu->current;
    cur->exit_code = exit_code;

    // Detach any of our children: their parent is about to be freed (or at
    // least become a zombie), so null their parent pointer to avoid
    // use-after-free in waitpid lookups. We walk every CPU's run queue,
    // current task, plus the zombie list. Tasks we orphan this way will
    // be auto-reaped by drain_pending_reap (parent==NULL → orphan path).
    uint64_t online = cpus_active ? cpus_active : 1;
    for (uint64_t i = 0; i < online; i++) {
        cpu_local_t* c = &cpu_locals[i];
        task_t* run = c->current;
        if (run && run->parent == cur) run->parent = NULL;
        run_queue_t* rq = &c->rq;
        spin_lock(&rq->lock);
        for (task_t* t = rq->head; t; t = t->next) {
            if (t->parent == cur) t->parent = NULL;
        }
        spin_unlock(&rq->lock);
    }
    spin_lock(&zombie_list_lock);
    for (task_t* t = zombie_list_head; t; t = t->zombie_next) {
        if (t->parent == cur) t->parent = NULL;
    }
    spin_unlock(&zombie_list_lock);

    cur->state = TASK_STATE_ZOMBIE;

    // If we have a parent, hand ourselves to it via the zombie list and
    // wake it if it's blocked on a matching waitpid. Orphans don't need
    // the list — drain_pending_reap will free them outright.
    if (cur->parent != NULL && cur->parent->state != TASK_STATE_ZOMBIE) {
        zombie_list_add(cur);
        task_t* p = cur->parent;
        if (p->state == TASK_STATE_BLOCKED &&
            (p->wait_target == 0 || p->wait_target == cur->pid)) {
            p->state = TASK_STATE_READY;
            rq_enqueue_external(p);
        }
    }

    schedule();
    // schedule() must not return — there is always at least the idle task
    // available. Belt-and-suspenders halt in case something goes wrong.
    while (true) HltHelper();
}

int task_has_children(task_t* parent, uint64_t target_pid) {
    uint64_t online = cpus_active ? cpus_active : 1;
    for (uint64_t i = 0; i < online; i++) {
        cpu_local_t* c = &cpu_locals[i];
        task_t* run = c->current;
        if (run && run->parent == parent &&
            (target_pid == 0 || run->pid == target_pid)) {
            return 1;
        }
        run_queue_t* rq = &c->rq;
        spin_lock(&rq->lock);
        for (task_t* t = rq->head; t; t = t->next) {
            if (t->parent == parent &&
                (target_pid == 0 || t->pid == target_pid)) {
                spin_unlock(&rq->lock);
                return 1;
            }
        }
        spin_unlock(&rq->lock);
    }
    spin_lock(&zombie_list_lock);
    for (task_t* t = zombie_list_head; t; t = t->zombie_next) {
        if (t->parent == parent &&
            (target_pid == 0 || t->pid == target_pid)) {
            spin_unlock(&zombie_list_lock);
            return 1;
        }
    }
    spin_unlock(&zombie_list_lock);
    return 0;
}

void scheduler_tick(void) {
    cpu_local_t* cpu = this_cpu();
    sleep_queue_wake_expired(cpu);
    task_t* cur = cpu->current;
    if (!cur) return;
    if (cur->time_slice > 0) cur->time_slice--;
    if (cur->time_slice == 0) {
        // Time's up — switch. Interrupts are already off (we're in IRQ ctx).
        schedule();
    }
}
