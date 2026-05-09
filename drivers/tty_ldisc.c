#include "tty_ldisc.h"
#include "tty_types.h"
#include "task_types.h"
#include "signal.h"

uint64_t ldisc_count(const ldisc_ring_t* r) {
    return *r->head - *r->tail;
}

void ldisc_push(const ldisc_ring_t* r, char c) {
    if (ldisc_count(r) >= r->size) return;
    r->buf[*r->head % r->size] = c;
    (*r->head)++;
}

bool ldisc_pop_back(const ldisc_ring_t* r, char* out) {
    if (ldisc_count(r) == 0) return false;
    (*r->head)--;
    if (out) *out = r->buf[*r->head % r->size];
    return true;
}

int64_t ldisc_drain(const ldisc_ring_t* r, uint32_t flags,
                    char* dst, uint64_t size) {
    uint64_t out = 0;
    if (flags & TTY_FLAG_ICANON) {
        bool has_nl = false;
        uint64_t nl_pos = 0;
        for (uint64_t i = *r->tail; i != *r->head; i++) {
            if (r->buf[i % r->size] == '\n') {
                has_nl = true;
                nl_pos = i;
                break;
            }
        }
        if (!has_nl) return 0;
        while (*r->tail <= nl_pos && out < size) {
            dst[out++] = r->buf[*r->tail % r->size];
            (*r->tail)++;
        }
        return (int64_t)out;
    }
    while (*r->tail != *r->head && out < size) {
        dst[out++] = r->buf[*r->tail % r->size];
        (*r->tail)++;
    }
    return (int64_t)out;
}

void ldisc_waiter_enqueue(const ldisc_ring_t* r, task_t* me) {
    me->next = NULL;
    me->prev = *r->waiter_tail;
    if (*r->waiter_tail) (*r->waiter_tail)->next = me;
    else                 *r->waiter = me;
    *r->waiter_tail = me;
}

task_t* ldisc_waiter_pop(const ldisc_ring_t* r) {
    task_t* w = *r->waiter;
    if (!w) return NULL;
    *r->waiter = w->next;
    if (*r->waiter) (*r->waiter)->prev = NULL;
    else            *r->waiter_tail = NULL;
    w->next = w->prev = NULL;
    return w;
}

void ldisc_waiter_remove(const ldisc_ring_t* r, task_t* me) {
    if (!(me->next || me->prev || *r->waiter == me)) return;
    if (me->prev)              me->prev->next = me->next;
    else if (*r->waiter == me) *r->waiter     = me->next;
    if (me->next)                   me->next->prev = me->prev;
    else if (*r->waiter_tail == me) *r->waiter_tail = me->prev;
    me->next = me->prev = NULL;
}

void ldisc_input(uint32_t flags,
                 const ldisc_ring_t* r,
                 uint64_t pgrp,
                 char byte,
                 void (*echo)(void* ctx, char c),
                 void* echo_ctx,
                 ldisc_result_t* out) {
    out->signal      = 0;
    out->signal_pgrp = 0;
    out->wake        = NULL;

    // Ctrl+C: consume byte, defer SIGINT delivery to the caller.
    if ((flags & TTY_FLAG_ISIG) && byte == 0x03) {
        out->signal      = SIGINT;
        out->signal_pgrp = pgrp;
        return;
    }

    // Cooked-mode backspace: erase last unread byte and echo the destructive
    // BS. Don't underflow into bytes a reader has already consumed.
    if ((flags & TTY_FLAG_ICANON) && byte == '\b') {
        char popped;
        if (ldisc_pop_back(r, &popped) && (flags & TTY_FLAG_ECHO) && echo) {
            echo(echo_ctx, '\b');
        }
        return;
    }

    ldisc_push(r, byte);

    if ((flags & TTY_FLAG_ECHO) && echo) {
        echo(echo_ctx, byte);
    }

    // ICANON readers can only progress on '\n'; raw mode unblocks on any
    // byte. Wake at most one — readers re-check on resume.
    bool wake = (flags & TTY_FLAG_ICANON) ? (byte == '\n') : true;
    if (wake) out->wake = ldisc_waiter_pop(r);
}
