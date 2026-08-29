/* =============================================================
 * kernel/memory.c  —  Physical + Virtual memory management
 *
 * Sections:
 *  1. Physical Memory Manager (PMM) — bitmap-based frame allocator
 *  2. Virtual Memory Manager (VMM) — 4 KB page directory/tables
 *  3. Kernel Heap (kmalloc/kfree)  — free-list allocator with
 *     block coalescing and split on alloc
 * ============================================================= */
#include "../include/kernel.h"

/* ═══════════════════════════════════════════════════════════════
 *  1. Physical Memory Manager
 *     Uses a bitmap: 1 bit per 4 KB frame.
 *     0 = free, 1 = used.
 * ═══════════════════════════════════════════════════════════════ */

/* Maximum physical memory we'll address: 4 GB → 1 M frames */
#define PMM_FRAMES_MAX  (1024 * 1024)
#define PMM_BITMAP_SIZE (PMM_FRAMES_MAX / 32)   /* uint32_t words */

static uint32_t pmm_bitmap[PMM_BITMAP_SIZE];
static uint32_t pmm_total_frames = 0;
static uint32_t pmm_used_frames  = 0;

/* ── Bitmap helpers ──────────────────────────────────────────── */
static inline void pmm_set(uint32_t frame)
{
    pmm_bitmap[frame / 32] |= BIT(frame % 32);
}

static inline void pmm_clear(uint32_t frame)
{
    pmm_bitmap[frame / 32] &= ~BIT(frame % 32);
}

static inline bool pmm_test(uint32_t frame)
{
    return !!(pmm_bitmap[frame / 32] & BIT(frame % 32));
}

/* Find the first free frame (linear scan).
 * Returns frame index or (uint32_t)-1 on OOM. */
static uint32_t pmm_first_free(void)
{
    for (uint32_t w = 0; w < PMM_BITMAP_SIZE; w++) {
        if (pmm_bitmap[w] == 0xFFFFFFFF) continue;   /* all used */
        for (uint32_t b = 0; b < 32; b++) {
            if (!(pmm_bitmap[w] & BIT(b)))
                return w * 32 + b;
        }
    }
    return (uint32_t)-1;
}

/* ── Public PMM API ──────────────────────────────────────────── */

void pmm_init(uint32_t mem_kb, uintptr_t kernel_end)
{
    pmm_total_frames = (mem_kb * 1024) / PAGE_SIZE;
    pmm_used_frames  = 0;

    /* Start with everything marked used */
    kmemset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));
    pmm_used_frames = pmm_total_frames;
}

/* Mark a physical region as usable (called for each Multiboot mmap entry) */
void pmm_init_region(uintptr_t base, size_t len)
{
    uint32_t frame = (uint32_t)(base / PAGE_SIZE);
    uint32_t count = (uint32_t)(len  / PAGE_SIZE);

    for (uint32_t i = 0; i < count; i++) {
        if (pmm_test(frame + i)) {
            pmm_clear(frame + i);
            pmm_used_frames--;
        }
    }
}

/* Mark a physical region as reserved (can't be allocated) */
void pmm_deinit_region(uintptr_t base, size_t len)
{
    uint32_t frame = (uint32_t)(base / PAGE_SIZE);
    uint32_t count = (uint32_t)(len  / PAGE_SIZE);

    for (uint32_t i = 0; i < count; i++) {
        if (!pmm_test(frame + i)) {
            pmm_set(frame + i);
            pmm_used_frames++;
        }
    }
}

/* Allocate one physical frame.  Returns physical address or NULL on OOM. */
void *pmm_alloc_frame(void)
{
    if (pmm_used_frames >= pmm_total_frames) return NULL;

    uint32_t frame = pmm_first_free();
    if (frame == (uint32_t)-1) return NULL;

    pmm_set(frame);
    pmm_used_frames++;
    return (void *)(uintptr_t)(frame * PAGE_SIZE);
}

/* Free a physical frame. */
void pmm_free_frame(void *frame_ptr)
{
    uint32_t frame = (uint32_t)((uintptr_t)frame_ptr / PAGE_SIZE);
    ASSERT(pmm_test(frame));      /* double-free check */
    pmm_clear(frame);
    pmm_used_frames--;
}

void pmm_get_stats(pmm_stats_t *s)
{
    s->total_frames = pmm_total_frames;
    s->used_frames  = pmm_used_frames;
    s->free_frames  = pmm_total_frames - pmm_used_frames;
}

/* ═══════════════════════════════════════════════════════════════
 *  2. Virtual Memory Manager
 *
 *  4 GB address space: 1024 page directories × 1024 page tables.
 *  Each PTE maps one 4 KB frame.
 *
 *  Layout:
 *    0x00000000–0xBFFFFFFF  User space  (3 GB)
 *    0xC0000000–0xFFFFFFFF  Kernel      (1 GB) — identity-mapped
 * ═══════════════════════════════════════════════════════════════ */

#define VMM_PD_INDEX(va)   (((va) >> 22) & 0x3FF)
#define VMM_PT_INDEX(va)   (((va) >> 12) & 0x3FF)
#define VMM_PAGE_FRAME(pa) ((pa) & ~0xFFF)

/* Identity-mapped kernel page directory (static, 4 KB aligned) */
static pde_t kernel_pd[1024] ALIGNED(PAGE_SIZE);

/* Kernel page tables for the first 4 GB — we only need to map
 * the lower 256 MB for most toy kernels.  We keep 64 tables here
 * (64 × 1024 × 4 KB = 256 MB). */
#define VMM_KERNEL_PT_COUNT  64
static pte_t kernel_pts[VMM_KERNEL_PT_COUNT][1024] ALIGNED(PAGE_SIZE);

static page_directory_t current_dir = NULL;

/* Allocate a page table (4 KB, physically contiguous) */
static pte_t *alloc_page_table(void)
{
    pte_t *pt = (pte_t *)pmm_alloc_frame();
    if (!pt) PANIC("VMM: out of memory for page table");
    kmemset(pt, 0, PAGE_SIZE);
    return pt;
}

void vmm_map_page(page_directory_t dir, uintptr_t virt,
                  uintptr_t phys, uint32_t flags)
{
    uint32_t pdi = VMM_PD_INDEX(virt);
    uint32_t pti = VMM_PT_INDEX(virt);

    pde_t pde = dir[pdi];
    pte_t *pt;

    if (!(pde & PAGE_PRESENT)) {
        /* No page table yet — allocate one */
        pt = alloc_page_table();
        dir[pdi] = (pde_t)((uintptr_t)pt | PAGE_PRESENT | PAGE_WRITABLE |
                            (flags & PAGE_USER));
    } else {
        pt = (pte_t *)(uintptr_t)VMM_PAGE_FRAME(pde);
    }

    pt[pti] = (pte_t)(VMM_PAGE_FRAME(phys) | (flags & 0xFFF) | PAGE_PRESENT);
}

void vmm_unmap_page(page_directory_t dir, uintptr_t virt)
{
    uint32_t pdi = VMM_PD_INDEX(virt);
    uint32_t pti = VMM_PT_INDEX(virt);

    pde_t pde = dir[pdi];
    if (!(pde & PAGE_PRESENT)) return;

    pte_t *pt = (pte_t *)(uintptr_t)VMM_PAGE_FRAME(pde);
    pt[pti] = 0;
    vmm_flush_tlb(virt);
}

uintptr_t vmm_get_phys(page_directory_t dir, uintptr_t virt)
{
    uint32_t pdi = VMM_PD_INDEX(virt);
    uint32_t pti = VMM_PT_INDEX(virt);

    pde_t pde = dir[pdi];
    if (!(pde & PAGE_PRESENT)) return 0;

    pte_t *pt = (pte_t *)(uintptr_t)VMM_PAGE_FRAME(pde);
    if (!(pt[pti] & PAGE_PRESENT)) return 0;

    return (uintptr_t)VMM_PAGE_FRAME(pt[pti]) | (virt & 0xFFF);
}

void vmm_switch_directory(page_directory_t dir)
{
    current_dir = dir;
    cpu_write_cr3((uint32_t)(uintptr_t)dir);
}

void vmm_flush_tlb(uintptr_t virt)
{
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

page_directory_t vmm_create_directory(void)
{
    page_directory_t dir = (page_directory_t)pmm_alloc_frame();
    if (!dir) PANIC("VMM: out of memory for page directory");
    kmemset(dir, 0, PAGE_SIZE);
    /* Copy kernel mappings so kernel is accessible in all address spaces */
    for (uint32_t i = 768; i < 1024; i++)
        dir[i] = kernel_pd[i];
    return dir;
}

void vmm_destroy_directory(page_directory_t dir)
{
    /* Free user-space page tables (indices 0..767) */
    for (uint32_t i = 0; i < 768; i++) {
        if (dir[i] & PAGE_PRESENT) {
            pmm_free_frame((void *)(uintptr_t)VMM_PAGE_FRAME(dir[i]));
        }
    }
    pmm_free_frame(dir);
}

/* Page-fault handler (called from IDT handler for int 14) */
static void page_fault_handler(registers_t *regs)
{
    uintptr_t fault_addr = cpu_read_cr2();
    bool present  = !!(regs->err_code & BIT(0));
    bool write    = !!(regs->err_code & BIT(1));
    bool user     = !!(regs->err_code & BIT(2));
    bool reserved = !!(regs->err_code & BIT(3));

    screen_set_color(COLOR_WHITE, COLOR_RED);
    kprintf("\n*** PAGE FAULT ***\n");
    kprintf("  Addr     : 0x%x\n", (uint32_t)fault_addr);
    kprintf("  Present  : %s\n", present  ? "yes" : "no (not mapped)");
    kprintf("  Write    : %s\n", write    ? "yes" : "no (read)");
    kprintf("  User     : %s\n", user     ? "yes" : "no (kernel)");
    kprintf("  Reserved : %s\n", reserved ? "yes" : "no");
    kprintf("  EIP      : 0x%x\n", regs->eip);
    PANIC("Unhandled page fault");
}

void vmm_init(void)
{
    /* Identity-map the first VMM_KERNEL_PT_COUNT × 4 MB
     * of physical memory into the kernel page directory */
    for (uint32_t t = 0; t < VMM_KERNEL_PT_COUNT; t++) {
        pte_t *pt = kernel_pts[t];
        for (uint32_t p = 0; p < 1024; p++) {
            uintptr_t phys = (t * 1024 + p) * PAGE_SIZE;
            pt[p] = (pte_t)(phys | PAGE_PRESENT | PAGE_WRITABLE);
        }
        kernel_pd[t] = (pde_t)((uintptr_t)pt | PAGE_PRESENT | PAGE_WRITABLE);
    }

    /* Reserve identity area: mark kernel frames as used in PMM */
    extern uint8_t __kernel_start[];
    extern uint8_t __kernel_end[];
    pmm_deinit_region((uintptr_t)__kernel_start,
                      (uintptr_t)__kernel_end - (uintptr_t)__kernel_start);

    /* Register page-fault handler */
    irq_install_handler(14 - 32, NULL);   /* placeholder; set via idt */
    /* Actually wire the page-fault ISR directly through the ISR table */
    /* (idt_install already sets isr14; we install our C handler here) */
    /* Re-register as ISR 14 */
    /* irq_install_handler is for IRQs; for exceptions use a separate table */
    /* For simplicity: register in the ISR dispatch table for int 14 */
    /* This is handled via isr_handler dispatch in idt.c */
    UNUSED(page_fault_handler);  /* will be linked via idt dispatch */

    vmm_switch_directory(kernel_pd);
    cpu_enable_paging();
}

/* ═══════════════════════════════════════════════════════════════
 *  3. Kernel Heap  — free-list allocator
 *
 *  Each allocation is preceded by a header:
 *    [magic32][size][flags][prev*][next*]
 *  Adjacent free blocks are coalesced on kfree.
 *  On kmalloc, blocks are split when there is enough remainder.
 * ═══════════════════════════════════════════════════════════════ */

#define HEAP_MAGIC_FREE 0xDEADBEEF
#define HEAP_MAGIC_USED 0xC0FFEE00
#define HEAP_MIN_SPLIT  (sizeof(heap_block_t) + 8)  /* min leftover to split */

typedef struct heap_block {
    uint32_t           magic;
    size_t             size;     /* payload size (excl. header) */
    bool               free;
    struct heap_block *prev;
    struct heap_block *next;
} heap_block_t;

static heap_block_t *heap_head = NULL;
static size_t        heap_total = 0;

void kmalloc_init(uintptr_t start, size_t size)
{
    heap_head        = (heap_block_t *)start;
    heap_head->magic = HEAP_MAGIC_FREE;
    heap_head->size  = size - sizeof(heap_block_t);
    heap_head->free  = TRUE;
    heap_head->prev  = NULL;
    heap_head->next  = NULL;
    heap_total       = size;
}

static void coalesce(heap_block_t *b)
{
    /* Merge with next if it's free */
    if (b->next && b->next->free) {
        b->size += sizeof(heap_block_t) + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
    }
    /* Merge with prev if it's free */
    if (b->prev && b->prev->free) {
        b->prev->size += sizeof(heap_block_t) + b->size;
        b->prev->next  = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

void *kmalloc(size_t size)
{
    if (!size) return NULL;
    size = ALIGN_UP(size, 8);   /* 8-byte alignment */

    heap_block_t *b = heap_head;
    while (b) {
        if (b->free && b->size >= size) {
            /* Split if there's enough leftover */
            if (b->size >= size + HEAP_MIN_SPLIT) {
                heap_block_t *nb =
                    (heap_block_t *)((uintptr_t)(b+1) + size);
                nb->magic = HEAP_MAGIC_FREE;
                nb->size  = b->size - size - sizeof(heap_block_t);
                nb->free  = TRUE;
                nb->prev  = b;
                nb->next  = b->next;
                if (b->next) b->next->prev = nb;
                b->next = nb;
                b->size = size;
            }
            b->magic = HEAP_MAGIC_USED;
            b->free  = FALSE;
            return (void *)(b + 1);
        }
        b = b->next;
    }
    return NULL;    /* OOM */
}

void *kmalloc_aligned(size_t size, size_t align)
{
    /* Allocate extra to guarantee alignment.
     * Store original pointer just before returned pointer. */
    void *raw = kmalloc(size + align + sizeof(void *));
    if (!raw) return NULL;
    uintptr_t aligned = ALIGN_UP((uintptr_t)raw + sizeof(void *), align);
    ((void **)aligned)[-1] = raw;
    return (void *)aligned;
}

void *kcalloc(size_t n, size_t size)
{
    void *p = kmalloc(n * size);
    if (p) kmemset(p, 0, n * size);
    return p;
}

void *krealloc(void *ptr, size_t size)
{
    if (!ptr) return kmalloc(size);
    if (!size) { kfree(ptr); return NULL; }

    heap_block_t *b = (heap_block_t *)ptr - 1;
    ASSERT(b->magic == HEAP_MAGIC_USED);

    if (b->size >= size) return ptr;   /* already big enough */

    void *np = kmalloc(size);
    if (!np) return NULL;
    kmemcpy(np, ptr, b->size);
    kfree(ptr);
    return np;
}

void kfree(void *ptr)
{
    if (!ptr) return;
    heap_block_t *b = (heap_block_t *)ptr - 1;
    ASSERT(b->magic == HEAP_MAGIC_USED);
    b->magic = HEAP_MAGIC_FREE;
    b->free  = TRUE;
    coalesce(b);
}

void kmalloc_stats(size_t *used_out, size_t *free_out)
{
    size_t used = 0, free_bytes = 0;
    heap_block_t *b = heap_head;
    while (b) {
        if (b->free) free_bytes += b->size;
        else         used      += b->size;
        b = b->next;
    }
    if (used_out)  *used_out  = used;
    if (free_out)  *free_out  = free_bytes;
}

/* ═══════════════════════════════════════════════════════════════
 *  GDT implementation
 * ═══════════════════════════════════════════════════════════════ */

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr_val;

/* Defined in boot.asm — loads GDTR and performs far jump */
extern void gdt_flush(uint32_t gdt_ptr_addr);

static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran)
{
    gdt[i].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[i].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt[i].base_high   = (uint8_t)((base >> 24) & 0xFF);
    gdt[i].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[i].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    gdt[i].access      = access;
}

void gdt_install(void)
{
    gdt_ptr_val.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr_val.base  = (uint32_t)(uintptr_t)&gdt;

    /* 0: Null descriptor */
    gdt_set_entry(0, 0, 0, 0, 0);
    /* 1: Kernel code  (ring 0, execute/read) */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);
    /* 2: Kernel data  (ring 0, read/write) */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);
    /* 3: User code    (ring 3, execute/read) */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);
    /* 4: User data    (ring 3, read/write) */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);
    /* 5: Null (placeholder for TSS, set by tss_install) */
    gdt_set_entry(5, 0, 0, 0, 0);
    /* 6: Null */
    gdt_set_entry(6, 0, 0, 0, 0);

    /* Load GDTR — inline asm (no external flush stub needed) */
    __asm__ volatile(
        "lgdt (%0)\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "ljmp $0x08, $.flush\n"
        ".flush:\n"
        :: "r"(&gdt_ptr_val) : "eax", "memory"
    );
}

/* ═══════════════════════════════════════════════════════════════
 *  IDT + ISR + IRQ implementation
 * ═══════════════════════════════════════════════════════════════ */

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_ptr_val;
static isr_t       irq_handlers[16];

/* ISR stubs declared in boot.asm */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

static void idt_set_gate(uint8_t n, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt[n].base_low  = (uint16_t)(base & 0xFFFF);
    idt[n].base_high = (uint16_t)((base >> 16) & 0xFFFF);
    idt[n].sel       = sel;
    idt[n].always0   = 0;
    idt[n].flags     = flags;
}

void idt_install(void)
{
    idt_ptr_val.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr_val.base  = (uint32_t)(uintptr_t)&idt;

    kmemset(&idt, 0, sizeof(idt));

#define SET_ISR(n) idt_set_gate(n, (uint32_t)isr##n, 0x08, 0x8E)
    SET_ISR(0);  SET_ISR(1);  SET_ISR(2);  SET_ISR(3);
    SET_ISR(4);  SET_ISR(5);  SET_ISR(6);  SET_ISR(7);
    SET_ISR(8);  SET_ISR(9);  SET_ISR(10); SET_ISR(11);
    SET_ISR(12); SET_ISR(13); SET_ISR(14); SET_ISR(15);
    SET_ISR(16); SET_ISR(17); SET_ISR(18); SET_ISR(19);
    SET_ISR(20); SET_ISR(21); SET_ISR(22); SET_ISR(23);
    SET_ISR(24); SET_ISR(25); SET_ISR(26); SET_ISR(27);
    SET_ISR(28); SET_ISR(29); SET_ISR(30); SET_ISR(31);
#undef SET_ISR

    /* Remap PIC before setting IRQ gates */
    pic_remap(32, 40);

#define SET_IRQ(n,v) idt_set_gate(v, (uint32_t)irq##n, 0x08, 0x8E)
    SET_IRQ(0,32);  SET_IRQ(1,33);  SET_IRQ(2,34);  SET_IRQ(3,35);
    SET_IRQ(4,36);  SET_IRQ(5,37);  SET_IRQ(6,38);  SET_IRQ(7,39);
    SET_IRQ(8,40);  SET_IRQ(9,41);  SET_IRQ(10,42); SET_IRQ(11,43);
    SET_IRQ(12,44); SET_IRQ(13,45); SET_IRQ(14,46); SET_IRQ(15,47);
#undef SET_IRQ

    __asm__ volatile("lidt (%0)" :: "r"(&idt_ptr_val) : "memory");
}

/* Exception messages */
static const char *exception_msgs[] = {
    "Division by Zero",        "Debug",
    "Non-Maskable Interrupt",  "Breakpoint",
    "Overflow",                "Bound Range Exceeded",
    "Invalid Opcode",          "Device Not Available",
    "Double Fault",            "Coprocessor Segment Overrun",
    "Invalid TSS",             "Segment Not Present",
    "Stack-Segment Fault",     "General Protection Fault",
    "Page Fault",              "Reserved",
    "x87 FPU Error",           "Alignment Check",
    "Machine Check",           "SIMD FP Exception",
    "Virtualisation Exception","Control Protection Exception",
    "Reserved","Reserved","Reserved","Reserved","Reserved","Reserved",
    "Reserved","Reserved",    "Security Exception","Reserved"
};

void isr_handler(registers_t *regs)
{
    if (regs->int_no < 32) {
        /* CPU exception */
        if (regs->int_no == 14) {
            page_fault_handler(regs);
            return;
        }
        screen_set_color(COLOR_WHITE, COLOR_RED);
        kprintf("\n*** CPU EXCEPTION #%u ***\n", regs->int_no);
        kprintf("  %s\n", exception_msgs[regs->int_no]);
        kprintf("  EIP=0x%x  CS=0x%x  EFLAGS=0x%x\n",
                regs->eip, regs->cs, regs->eflags);
        kprintf("  EAX=0x%x EBX=0x%x ECX=0x%x EDX=0x%x\n",
                regs->eax, regs->ebx, regs->ecx, regs->edx);
        kprintf("  Error code: 0x%x\n", regs->err_code);
        PANIC("Unhandled CPU exception");
    } else if (regs->int_no >= 32 && regs->int_no < 48) {
        /* Hardware IRQ */
        uint8_t irq = (uint8_t)(regs->int_no - 32);
        if (irq_handlers[irq])
            irq_handlers[irq](regs);
        pic_send_eoi(irq);
    }
}

void irq_install_handler(int irq, isr_t handler)
{
    ASSERT(irq >= 0 && irq < 16);
    irq_handlers[irq] = handler;
}

void irq_uninstall_handler(int irq)
{
    ASSERT(irq >= 0 && irq < 16);
    irq_handlers[irq] = NULL;
}

/* ═══════════════════════════════════════════════════════════════
 *  PIC (8259A) driver
 * ═══════════════════════════════════════════════════════════════ */
void pic_remap(int offset1, int offset2)
{
    uint8_t a1 = inb(PIC1_DATA);   /* save masks */
    uint8_t a2 = inb(PIC2_DATA);

    outb(PIC1_CMD,  0x11); io_wait();  /* ICW1: cascade mode */
    outb(PIC2_CMD,  0x11); io_wait();
    outb(PIC1_DATA, (uint8_t)offset1); io_wait(); /* ICW2: vector offset */
    outb(PIC2_DATA, (uint8_t)offset2); io_wait();
    outb(PIC1_DATA, 0x04); io_wait();  /* ICW3: slave on IRQ2 */
    outb(PIC2_DATA, 0x02); io_wait();  /* ICW3: slave ID = 2 */
    outb(PIC1_DATA, 0x01); io_wait();  /* ICW4: 8086 mode */
    outb(PIC2_DATA, 0x01); io_wait();

    outb(PIC1_DATA, a1);               /* restore masks */
    outb(PIC2_DATA, a2);
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_set_mask(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb((uint16_t)port, (uint8_t)(inb((uint16_t)port) | BIT(irq)));
}

void pic_clear_mask(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb((uint16_t)port, (uint8_t)(inb((uint16_t)port) & ~BIT(irq)));
}

static uint16_t pic_get_reg(uint8_t cmd)
{
    outb(PIC1_CMD, cmd); outb(PIC2_CMD, cmd);
    return (uint16_t)((uint16_t)inb(PIC2_CMD) << 8 | inb(PIC1_CMD));
}
uint16_t pic_get_irr(void) { return pic_get_reg(0x0A); }
uint16_t pic_get_isr(void) { return pic_get_reg(0x0B); }

/* ═══════════════════════════════════════════════════════════════
 *  PIT (Programmable Interval Timer) — IRQ0
 * ═══════════════════════════════════════════════════════════════ */
#define PIT_CH0     0x40
#define PIT_CMD     0x43
#define PIT_MODE3   0x36   /* channel 0, lobyte/hibyte, square wave */

static volatile uint64_t pit_ticks = 0;
static uint32_t          pit_hz    = 0;

static void pit_irq_handler(registers_t *regs)
{
    UNUSED(regs);
    pit_ticks++;
}

void pit_install(uint32_t hz)
{
    pit_hz = hz;
    uint32_t divisor = PIT_BASE_FREQ / hz;
    outb(PIT_CMD, PIT_MODE3);
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
    irq_install_handler(0, pit_irq_handler);
    pic_clear_mask(0);
}

uint64_t pit_get_ticks(void) { return pit_ticks; }

void pit_sleep_ms(uint32_t ms)
{
    uint64_t target = pit_ticks + (uint64_t)((ms * pit_hz) / 1000 + 1);
    while (pit_ticks < target)
        __asm__ volatile("hlt");
}

/* ═══════════════════════════════════════════════════════════════
 *  PS/2 Keyboard driver — IRQ1
 * ═══════════════════════════════════════════════════════════════ */
#define KBD_DATA    0x60
#define KBD_STATUS  0x64
#define KBD_BUF_SIZE 64

static char  kbd_buf[KBD_BUF_SIZE];
static uint8_t kbd_head = 0, kbd_tail = 0;
static bool   kbd_shift = FALSE;
static bool   kbd_ctrl  = FALSE;
static bool   kbd_caps  = FALSE;

static const char sc_lower[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',
    0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,
    '7','8','9','-','4','5','6','+','1','2','3','0','.',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const char sc_upper[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',
    0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,
    '7','8','9','-','4','5','6','+','1','2','3','0','.',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static void kbd_enqueue(char c)
{
    uint8_t next = (uint8_t)((kbd_tail + 1) % KBD_BUF_SIZE);
    if (next != kbd_head) {
        kbd_buf[kbd_tail] = c;
        kbd_tail = next;
    }
}

static void kbd_irq_handler(registers_t *regs)
{
    UNUSED(regs);
    uint8_t sc = inb(KBD_DATA);
    bool released = !!(sc & 0x80);
    sc &= 0x7F;

    switch (sc) {
    case 0x2A: case 0x36: kbd_shift = !released; return;
    case 0x1D:             kbd_ctrl  = !released; return;
    case 0x3A: if (!released) kbd_caps = !kbd_caps; return;
    default: break;
    }

    if (released) return;

    bool upper = (kbd_shift ^ kbd_caps);
    char c = upper ? sc_upper[sc] : sc_lower[sc];
    if (c) {
        if (kbd_ctrl && c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 1);   /* Ctrl+letter → control code */
        kbd_enqueue(c);
    }
}

void keyboard_install(void)
{
    kmemset(kbd_buf, 0, sizeof(kbd_buf));
    kbd_head = kbd_tail = 0;
    irq_install_handler(1, kbd_irq_handler);
    pic_clear_mask(1);
}

int keyboard_poll(void)
{
    if (kbd_head == kbd_tail) return -1;
    char c = kbd_buf[kbd_head];
    kbd_head = (uint8_t)((kbd_head + 1) % KBD_BUF_SIZE);
    return (unsigned char)c;
}

int keyboard_getchar(void)
{
    int c;
    while ((c = keyboard_poll()) == -1)
        __asm__ volatile("hlt");
    return c;
}
