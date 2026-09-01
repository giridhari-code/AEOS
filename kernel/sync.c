/*
 * AEOS - Kernel synchronization primitives
 */

#include "sync.h"
#include "sched.h"
#include "memory.h"

/* Current task index from the scheduler (sched.c). */
extern int sched_current_index(void);

/* ============================================================
 * Mutex
 * ============================================================ */

void mutex_init(mutex_t *m)
{
    m->owner = -1;
    m->lock_count = 0;
}

int mutex_trylock(mutex_t *m)
{
    int me = sched_current_index();
    int got;

    irq_disable();
    if (m->owner == -1 || m->owner == me) {
        m->owner = me;
        got = 1;
    } else {
        m->lock_count++;
        got = 0;
    }
    irq_enable();
    return got;
}

void mutex_lock(mutex_t *m)
{
    /* Cooperative blocking: spin on the scheduler, not the CPU. */
    while (!mutex_trylock(m))
        sched_yield();
}

void mutex_unlock(mutex_t *m)
{
    irq_disable();
    if (m->owner == sched_current_index())
        m->owner = -1;
    irq_enable();
}

/* ============================================================
 * Counting semaphore
 * ============================================================ */

void sem_init(semaphore_t *s, int32_t initial)
{
    s->count = initial;
    s->wakeups = 0;
}

int sem_trywait(semaphore_t *s)
{
    int got;

    irq_disable();
    if (s->count > 0) {
        s->count--;
        got = 1;
    } else {
        got = 0;
    }
    irq_enable();
    return got;
}

void sem_wait(semaphore_t *s)
{
    while (!sem_trywait(s))
        sched_yield();
}

void sem_post(semaphore_t *s)
{
    irq_disable();
    s->count++;
    s->wakeups++;
    irq_enable();
}
