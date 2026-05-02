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

#define DEFAULT_TIME_SLICE 20   // ticks (1 ms each) — 20 ms quantum


typedef struct run_queue_t {
    task_t* head;
    task_t* tail;
    spinlock_t lock;
} run_queue_t;

static run_queue_t global_rq;

static inline run_queue_t* rq_for_this_cpu(void) {
    return &global_rq;
}

// Hook: which queue should a (newly-ready or re-enqueued) task land on?
// `t` may be NULL when we just want "some queue" for init. Per-CPU schedulers
// will pick based on t->cpu_affinity / the calling CPU.
static inline run_queue_t* rq_for_task(task_t* t) {
    (void)t;
    return &global_rq;
}

static uint64_t next_pid = 1;
static spinlock_t pid_lock = {0};

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

void SchedulerInit(void) {
    rq_init(&global_rq);
    pid_lock.locked = 0;
    next_pid = 1;
}

task_t* create_kernel_task(void (*entry)(void), const char* name) {
    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    memset(t, 0, sizeof(task_t));

    uint64_t stack_base = AddStack();              // virtual addr (low end)
    uint64_t stack_top  = stack_base + STACK_PAGES * PAGE_SIZE;

    t->pid          = alloc_pid();
    t->state        = TASK_STATE_READY;
    t->cr3          = KERNEL_VIRT_TO_PHYS(PageDirAddrV());
    t->kstack_base  = stack_base;
    t->kstack_top   = stack_top;
    t->kstack_pages = STACK_PAGES;
    t->time_slice   = DEFAULT_TIME_SLICE;

    if (name) {
        for (uint64_t i = 0; i < 31 && name[i]; i++) t->name[i] = name[i];
    }

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
    if (name) {
        for (int i = 0; i < 31 && name[i]; i++) t->name[i] = name[i];
    }
    this_cpu()->current = t;
}

void schedule(void) {
    bool ie = check_interrupts();
    CliHelper();

    cpu_local_t* cpu = this_cpu();
    task_t* prev = cpu->current;
    run_queue_t* rq = rq_for_this_cpu();

    spin_lock(&rq->lock);
    task_t* next = rq_dequeue_locked(rq);
    if (!next) {
        // Nothing to run: keep prev. Common during early bootstrap before
        // any tasks exist. Just unlock and return.
        spin_unlock(&rq->lock);
        if (ie) StiHelper();
        return;
    }
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

    if (prev != next) context_switch(prev, next);

    // Resumed: restore IF if it was on at the call site.
    if (ie) StiHelper();
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
