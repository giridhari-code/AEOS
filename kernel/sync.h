/*
 * AEOS - Kernel synchronization primitives (public API)
 *
 * Mutex and counting semaphore for kernel tasks.
 *
 * Blocking is cooperative: a task that cannot acquire a primitive
 * yields and retries. There is no waiter queue in Phase 0 - the
 * round-robin scheduler makes retry loops fair.
 *
 * All state transitions are IRQ-safe (short DAIF-masked windows).
 */

#ifndef AEOS_SYNC_H
#define AEOS_SYNC_H

#include <stdint.h>

typedef struct {
    int      owner;      /* task index holding the lock, -1 = free */
    uint32_t lock_count; /* contention counter (diagnostics) */
} mutex_t;

typedef struct {
    int32_t  count;
    uint32_t wakeups;    /* post() counter (diagnostics) */
} semaphore_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);     /* blocks (yields) until acquired */
int  mutex_trylock(mutex_t *m);  /* 1 = acquired, 0 = busy */
void mutex_unlock(mutex_t *m);

void sem_init(semaphore_t *s, int32_t initial);
void sem_wait(semaphore_t *s);   /* blocks (yields) until count > 0 */
int  sem_trywait(semaphore_t *s);
void sem_post(semaphore_t *s);

#endif /* AEOS_SYNC_H */
