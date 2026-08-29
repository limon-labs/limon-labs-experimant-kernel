#include "kernel.h"

void semaphore_init(semaphore_t *sem, uint32_t max) {
    sem->count = max;
    sem->max = max;
    sem->waiters = 0;
}

void semaphore_wait(semaphore_t *sem) {
    while (sem->count == 0) {
        sem->waiters++;
        __asm__ volatile("pause");
    }
    sem->count--;
}

void semaphore_signal(semaphore_t *sem) {
    if (sem->count < sem->max) {
        sem->count++;
        if (sem->waiters > 0) sem->waiters--;
    }
}
