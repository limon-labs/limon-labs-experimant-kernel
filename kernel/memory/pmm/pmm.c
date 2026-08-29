#include "kernel.h"

#define PMM_FRAMES_MAX  (1024 * 1024)
#define PMM_BITMAP_SIZE (PMM_FRAMES_MAX / 32)

static uint32_t pmm_bitmap[PMM_BITMAP_SIZE];
static uint32_t pmm_total_frames = 0;
static uint32_t pmm_used_frames = 0;

static inline void pmm_set(uint32_t frame) {
    pmm_bitmap[frame / 32] |= BIT(frame % 32);
}

static inline void pmm_clear(uint32_t frame) {
    pmm_bitmap[frame / 32] &= ~BIT(frame % 32);
}

static inline bool pmm_test(uint32_t frame) {
    return !!(pmm_bitmap[frame / 32] & BIT(frame % 32));
}

static uint32_t pmm_first_free(void) {
    for (uint32_t w = 0; w < PMM_BITMAP_SIZE; w++) {
        if (pmm_bitmap[w] == 0xFFFFFFFF) continue;
        for (uint32_t b = 0; b < 32; b++) {
            if (!(pmm_bitmap[w] & BIT(b)))
                return w * 32 + b;
        }
    }
    return (uint32_t)-1;
}

void pmm_init(uint32_t mem_kb, uintptr_t kernel_end)
{
    (void)kernel_end;  /* Mark as unused to prevent warning */
    
    pmm_total_frames = (mem_kb * 1024) / PAGE_SIZE;
    pmm_used_frames = pmm_total_frames;
    kmemset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));
}

void pmm_init_region(uintptr_t base, size_t len) {
    uint32_t frame = (uint32_t)(base / PAGE_SIZE);
    uint32_t count = (uint32_t)(len / PAGE_SIZE);
    for (uint32_t i = 0; i < count; i++) {
        if (pmm_test(frame + i)) {
            pmm_clear(frame + i);
            pmm_used_frames--;
        }
    }
}

void pmm_deinit_region(uintptr_t base, size_t len) {
    uint32_t frame = (uint32_t)(base / PAGE_SIZE);
    uint32_t count = (uint32_t)(len / PAGE_SIZE);
    for (uint32_t i = 0; i < count; i++) {
        if (!pmm_test(frame + i)) {
            pmm_set(frame + i);
            pmm_used_frames++;
        }
    }
}

void *pmm_alloc_frame(void) {
    if (pmm_used_frames >= pmm_total_frames) return NULL;
    uint32_t frame = pmm_first_free();
    if (frame == (uint32_t)-1) return NULL;
    pmm_set(frame);
    pmm_used_frames++;
    return (void *)(frame * PAGE_SIZE);
}

void pmm_free_frame(void *frame) {
    uint32_t f = (uint32_t)((uintptr_t)frame / PAGE_SIZE);
    ASSERT(pmm_test(f));
    pmm_clear(f);
    pmm_used_frames--;
}

void pmm_get_stats(pmm_stats_t *s) {
    s->total_frames = pmm_total_frames;
    s->used_frames = pmm_used_frames;
    s->free_frames = pmm_total_frames - pmm_used_frames;
}
