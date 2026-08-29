#include "kernel.h"

void spinlock_init(spinlock_t *lock) {
    lock->locked = FALSE;
    lock->owner = 0;
    lock->spin_count = 0;
}

void spinlock_acquire(spinlock_t *lock) {
    while (lock->locked) {
        lock->spin_count++;
        __asm__ volatile("pause");
    }
    lock->locked = TRUE;
    lock->owner = 1;
}

void spinlock_release(spinlock_t *lock) {
    lock->locked = FALSE;
    lock->owner = 0;
}
