#include "kernel.h"

#define HEAP_MAGIC_FREE 0xDEADBEEF
#define HEAP_MAGIC_USED 0xC0FFEE00
#define HEAP_MIN_SPLIT  (sizeof(heap_block_t) + 8)

typedef struct heap_block {
    uint32_t magic;
    size_t size;
    bool free;
    struct heap_block *prev;
    struct heap_block *next;
} heap_block_t;

static heap_block_t *heap_head = NULL;
static size_t heap_total = 0;

void kmalloc_init(uintptr_t start, size_t size) {
    heap_head = (heap_block_t *)start;
    heap_head->magic = HEAP_MAGIC_FREE;
    heap_head->size = size - sizeof(heap_block_t);
    heap_head->free = TRUE;
    heap_head->prev = NULL;
    heap_head->next = NULL;
    heap_total = size;
}

static void coalesce(heap_block_t *b) {
    if (b->next && b->next->free) {
        b->size += sizeof(heap_block_t) + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
    }
    if (b->prev && b->prev->free) {
        b->prev->size += sizeof(heap_block_t) + b->size;
        b->prev->next = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

void *kmalloc(size_t size) {
    if (!size) return NULL;
    size = ALIGN_UP(size, 8);
    heap_block_t *b = heap_head;
    while (b) {
        if (b->free && b->size >= size) {
            if (b->size >= size + HEAP_MIN_SPLIT) {
                heap_block_t *nb = (heap_block_t *)((uintptr_t)(b+1) + size);
                nb->magic = HEAP_MAGIC_FREE;
                nb->size = b->size - size - sizeof(heap_block_t);
                nb->free = TRUE;
                nb->prev = b;
                nb->next = b->next;
                if (b->next) b->next->prev = nb;
                b->next = nb;
                b->size = size;
            }
            b->magic = HEAP_MAGIC_USED;
            b->free = FALSE;
            return (void *)(b + 1);
        }
        b = b->next;
    }
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;
    heap_block_t *b = (heap_block_t *)ptr - 1;
    ASSERT(b->magic == HEAP_MAGIC_USED);
    b->magic = HEAP_MAGIC_FREE;
    b->free = TRUE;
    coalesce(b);
}

void *kcalloc(size_t n, size_t size) {
    void *p = kmalloc(n * size);
    if (p) kmemset(p, 0, n * size);
    return p;
}

void *krealloc(void *ptr, size_t size) {
    if (!ptr) return kmalloc(size);
    if (!size) { kfree(ptr); return NULL; }
    heap_block_t *b = (heap_block_t *)ptr - 1;
    ASSERT(b->magic == HEAP_MAGIC_USED);
    if (b->size >= size) return ptr;
    void *np = kmalloc(size);
    if (!np) return NULL;
    kmemcpy(np, ptr, b->size);
    kfree(ptr);
    return np;
}

void kmalloc_stats(size_t *used, size_t *free_bytes) {
    size_t u = 0, f = 0;
    heap_block_t *b = heap_head;
    while (b) {
        if (b->free) f += b->size;
        else u += b->size;
        b = b->next;
    }
    if (used) *used = u;
    if (free_bytes) *free_bytes = f;
}
