#include "kernel.h"

static pde_t kernel_pd[1024] ALIGNED(PAGE_SIZE);
static pte_t kernel_pts[64][1024] ALIGNED(PAGE_SIZE);

static pte_t *alloc_page_table(void) {
    pte_t *pt = (pte_t *)pmm_alloc_frame();
    if (!pt) PANIC("VMM: Out of memory for page table");
    kmemset(pt, 0, PAGE_SIZE);
    return pt;
}

void vmm_map_page(page_directory_t dir, uintptr_t virt, uintptr_t phys, uint32_t flags) {
    uint32_t pdi = (virt >> 22) & 0x3FF;
    uint32_t pti = (virt >> 12) & 0x3FF;
    pde_t pde = dir[pdi];
    pte_t *pt;
    if (!(pde & PAGE_PRESENT)) {
        pt = alloc_page_table();
        dir[pdi] = (pde_t)((uintptr_t)pt | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER));
    } else {
        pt = (pte_t *)(uintptr_t)(pde & ~0xFFF);
    }
    pt[pti] = (pte_t)((phys & ~0xFFF) | (flags & 0xFFF) | PAGE_PRESENT);
}

void vmm_unmap_page(page_directory_t dir, uintptr_t virt) {
    uint32_t pdi = (virt >> 22) & 0x3FF;
    uint32_t pti = (virt >> 12) & 0x3FF;
    pde_t pde = dir[pdi];
    if (!(pde & PAGE_PRESENT)) return;
    pte_t *pt = (pte_t *)(uintptr_t)(pde & ~0xFFF);
    pt[pti] = 0;
    vmm_flush_tlb(virt);
}

uintptr_t vmm_get_phys(page_directory_t dir, uintptr_t virt) {
    uint32_t pdi = (virt >> 22) & 0x3FF;
    uint32_t pti = (virt >> 12) & 0x3FF;
    pde_t pde = dir[pdi];
    if (!(pde & PAGE_PRESENT)) return 0;
    pte_t *pt = (pte_t *)(uintptr_t)(pde & ~0xFFF);
    if (!(pt[pti] & PAGE_PRESENT)) return 0;
    return (uintptr_t)(pt[pti] & ~0xFFF) | (virt & 0xFFF);
}

void vmm_switch_directory(page_directory_t dir) {
    cpu_write_cr3((uint32_t)(uintptr_t)dir);
}

void vmm_flush_tlb(uintptr_t virt) {
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

page_directory_t vmm_create_directory(void) {
    page_directory_t dir = (page_directory_t)pmm_alloc_frame();
    if (!dir) PANIC("VMM: Out of memory for page directory");
    kmemset(dir, 0, PAGE_SIZE);
    for (uint32_t i = 768; i < 1024; i++)
        dir[i] = kernel_pd[i];
    return dir;
}

void vmm_destroy_directory(page_directory_t dir) {
    for (uint32_t i = 0; i < 768; i++) {
        if (dir[i] & PAGE_PRESENT)
            pmm_free_frame((void *)(uintptr_t)(dir[i] & ~0xFFF));
    }
    pmm_free_frame(dir);
}

void vmm_init(void) {
    for (uint32_t t = 0; t < 64; t++) {
        for (uint32_t p = 0; p < 1024; p++) {
            kernel_pts[t][p] = (t * 1024 + p) * PAGE_SIZE | PAGE_PRESENT | PAGE_WRITABLE;
        }
        kernel_pd[t] = (pde_t)((uintptr_t)kernel_pts[t] | PAGE_PRESENT | PAGE_WRITABLE);
    }
    vmm_switch_directory(kernel_pd);
    cpu_enable_paging();
}
