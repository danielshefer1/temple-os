#pragma once
#include "includes.h"

struct task_t;

// A view onto a ring buffer + its blocked-reader queue. Both tty_t and
// pty_pair_t carry inline buffers / waiter lists; they construct one of
// these on the stack to call into the shared line discipline.
//
// All fields are pointers — the helpers do not own storage. The caller
// is responsible for whatever lock guards these members.
typedef struct ldisc_ring_t {
    char*           buf;
    uint64_t        size;          // capacity in bytes
    uint64_t*       head;          // monotonic; index = *head % size
    uint64_t*       tail;          // monotonic
    struct task_t** waiter;        // head of FIFO blocked-reader queue
    struct task_t** waiter_tail;
} ldisc_ring_t;

// Out-of-lock side effects produced by ldisc_input. Caller must drop the
// ring lock before acting on these (signal_send_pgrp and rq_enqueue_external
// take other locks).
typedef struct ldisc_result_t {
    int             signal;        // 0 = none
    uint64_t        signal_pgrp;
    struct task_t*  wake;          // NULL if no waiter to wake
} ldisc_result_t;

// ---- ring helpers (caller holds lock) ------------------------------------

uint64_t ldisc_count(const ldisc_ring_t* r);
void     ldisc_push(const ldisc_ring_t* r, char c);          // drops on overflow
bool     ldisc_pop_back(const ldisc_ring_t* r, char* out);

// Drain into dst per `flags`. ICANON: copy up through the first '\n', or
// return 0 if no '\n' is buffered. Otherwise: drain whatever is buffered up
// to `size`. Returns bytes copied.
int64_t  ldisc_drain(const ldisc_ring_t* r, uint32_t flags,
                     char* dst, uint64_t size);

// ---- waiter queue (caller holds lock) ------------------------------------

void     ldisc_waiter_enqueue(const ldisc_ring_t* r, struct task_t* me);
struct task_t* ldisc_waiter_pop(const ldisc_ring_t* r);
// Splice `me` out of the queue if present (idempotent). Used by readers
// that resumed via signal rather than a producer wakeup.
void     ldisc_waiter_remove(const ldisc_ring_t* r, struct task_t* me);

// ---- input processing (caller holds lock) --------------------------------

// Process one input byte. With TTY_FLAG_ISIG and Ctrl+C, the byte is
// consumed and out->signal is set. With TTY_FLAG_ICANON and '\b', the
// last unread byte is erased (and echoed if TTY_FLAG_ECHO). Otherwise
// the byte is pushed; if TTY_FLAG_ECHO it is echoed; if a reader is
// blocked and the wake rule fires (ICANON: '\n' only; raw: every byte)
// one waiter is popped and returned in out->wake.
//
// `echo` may be NULL.
void ldisc_input(uint32_t flags,
                 const ldisc_ring_t* r,
                 uint64_t pgrp,
                 char byte,
                 void (*echo)(void* ctx, char c),
                 void* echo_ctx,
                 ldisc_result_t* out);
