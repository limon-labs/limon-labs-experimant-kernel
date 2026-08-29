#include "kernel.h"

static task_t *ready_queue = NULL;
static task_t *current_task = NULL;
static uint32_t next_task_id = 1;

void scheduler_init(void) {
    ready_queue = NULL;
    current_task = NULL;
    next_task_id = 1;
}

void scheduler_add_task(task_t *task) {
    task->id = next_task_id++;
    task->state = TASK_READY;
    task->next = ready_queue;
    ready_queue = task;
}

void scheduler_remove_task(uint32_t task_id) {
    task_t *prev = NULL;
    task_t *curr = ready_queue;
    while (curr) {
        if (curr->id == task_id) {
            if (prev) prev->next = curr->next;
            else ready_queue = curr->next;
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

void scheduler_yield(void) {
    if (!ready_queue) return;
    if (current_task) {
        current_task->state = TASK_READY;
        current_task->next = ready_queue;
        ready_queue = ready_queue->next;
    }
    current_task = ready_queue;
    ready_queue = ready_queue->next;
    current_task->state = TASK_RUNNING;
}

void scheduler_sleep(uint32_t ms) {
    (void)ms;  /* Mark as unused to prevent warning */
    if (current_task) {
        current_task->state = TASK_SLEEPING;
        scheduler_yield();
    }
}

void scheduler_wakeup(uint32_t task_id) {
    task_t *t = ready_queue;
    while (t) {
        if (t->id == task_id) {
            t->state = TASK_READY;
            return;
        }
        t = t->next;
    }
}

task_t *scheduler_current(void) {
    return current_task;
}
