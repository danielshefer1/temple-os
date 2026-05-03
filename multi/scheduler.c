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
#include <stddef.h>

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

void SchedulerInit(void) {
    for (uint32_t i = 0; i < MAX_CPUS; i++) {
        rq_init(&cpu_locals[i].rq);
    }
    pid_lock.locked = 0;
    next_pid = 1;
    next_assign_cpu = 0;
}

task_t* alloc_blank_task(const char* name) {
    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    memset(t, 0, sizeof(task_t));

    uint64_t stack_base = AddStack();
    uint64_t stack_top  = stack_base + STACK_PAGES * PAGE_SIZE;

    t->pid          = alloc_pid();
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

// Free a zombie left behind by a previous context_switch on this CPU. Safe
// to call now because context_switch has long since moved off the zombie's
// stack — we're running on some other task's stack at this point.
static void drain_pending_reap(cpu_local_t* cpu) {
    task_t* dead = cpu->pending_reap;
    if (!dead) return;
    cpu->pending_reap = NULL;
    if (dead->kstack_pages) {
        RemoveKernelPages(dead->kstack_base, dead->kstack_pages);
    }
    kfree(dead, sizeof(task_t));
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
    if (prev && prev->state == TASK_STATE_ZOMBIE) {
        cpu->pending_reap = prev;
    }

    if (prev != next) context_switch(prev, next);

    // Resumed: restore IF if it was on at the call site.
    if (ie) StiHelper();
}

void task_exit(void) {
    CliHelper();
    cpu_local_t* cpu = this_cpu();
    task_t* cur = cpu->current;
    cur->state = TASK_STATE_ZOMBIE;
    schedule();
    // schedule() must not return — there is always at least the idle task
    // available. Belt-and-suspenders halt in case something goes wrong.
    while (true) HltHelper();
}

void scheduler_tick(void) {
    cpu_local_t* cpu = this_cpu();
    task_t* cur = cpu->current;
    if (!cur) return;
    if (cur->time_slice > 0) cur->time_slice--;
    if (cur->time_slice == 0) {
        // Time's up — switch. Interrupts are already off (we're in IRQ ctx).
        schedule();
    }
}
