#include "mutex.h"
#include "scheduler.h"
#include "cpu_local.h"
#include "extern.h"

void mutex_init(mutex_t* m) {
    m->guard.locked = 0;
    m->locked = 0;
    m->owner = NULL;
    m->wait_head = NULL;
    m->wait_tail = NULL;
}

void mutex_lock(mutex_t* m) {
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&m->guard);

    if (!m->locked) {
        m->locked = 1;
        m->owner = this_cpu()->current;
        spin_unlock(&m->guard);
        if (ie) StiHelper();
        return;
    }

    task_t* me = this_cpu()->current;
    me->next = NULL;
    me->prev = m->wait_tail;
    if (m->wait_tail) m->wait_tail->next = me;
    else              m->wait_head = me;
    m->wait_tail = me;

    me->state = TASK_STATE_BLOCKED;

    spin_unlock(&m->guard);
    schedule();
    if (ie) StiHelper();
}

void mutex_unlock(mutex_t* m) {
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&m->guard);

    task_t* waiter = m->wait_head;
    if (waiter) {
        m->wait_head = waiter->next;
        if (m->wait_head) m->wait_head->prev = NULL;
        else              m->wait_tail = NULL;
        waiter->next = waiter->prev = NULL;

        m->owner = waiter;
        waiter->state = TASK_STATE_READY;
        spin_unlock(&m->guard);

        rq_enqueue_external(waiter);
    } else {
        m->locked = 0;
        m->owner = NULL;
        spin_unlock(&m->guard);
    }

    if (ie) StiHelper();
}
