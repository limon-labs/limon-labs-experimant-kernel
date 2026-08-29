#include "kernel.h"

void mutex_init(mutex_t *mutex) {
    mutex->locked = FALSE;
    mutex->owner = 0;
    mutex->waiters = 0;
}

void mutex_lock(mutex_t *mutex) {
    while (mutex->locked) {
        mutex->waiters++;
        __asm__ volatile("pause");
    }
    mutex->locked = TRUE;
    mutex->owner = 1;
}

void mutex_unlock(mutex_t *mutex) {
    mutex->locked = FALSE;
    mutex->owner = 0;
    if (mutex->waiters > 0) mutex->waiters--;
}
