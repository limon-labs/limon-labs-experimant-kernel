/* =============================================================
 * include/kernel.h  —  Master kernel header
 * Every subsystem includes this file.
 * ============================================================= */
#ifndef KERNEL_H
#define KERNEL_H

/* ── Compiler / ABI helpers ─────────────────────────────────── */
#define NULL            ((void*)0)
#define TRUE            1
#define FALSE           0
#define UNUSED(x)       ((void)(x))
#define PACKED          __attribute__((packed))
#define NORETURN        __attribute__((noreturn))
#define ALIGNED(n)      __attribute__((aligned(n)))
#define SECTION(s)      __attribute__((section(s)))
#define LIKELY(x)       __builtin_expect(!!(x), 1)
#define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))
#define MAX(a,b)        ((a) > (b) ? (a) : (b))
#define MIN(a,b)        ((a) < (b) ? (a) : (b))
#define ALIGN_UP(x,a)   (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x,a) ((x) & ~((a) - 1))
#define BIT(n)          (1U << (n))
#define KB(n)           ((n) * 1024U)
#define MB(n)           ((n) * 1024U * 1024U)

/* ── Fixed-width integer types ───────────────────────────────── */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;
typedef uint32_t           size_t;
typedef int32_t            ssize_t;
typedef uint32_t           uintptr_t;
typedef int32_t            intptr_t;
typedef uint8_t            bool;

/* ── I/O port functions (asm stubs in boot.asm) ─────────────── */
void     outb(uint16_t port, uint8_t  val);
uint8_t  inb (uint16_t port);
void     outw(uint16_t port, uint16_t val);
uint16_t inw (uint16_t port);
void     outl(uint16_t port, uint32_t val);
uint32_t inl (uint16_t port);
void     io_wait(void);

/* ── CPU control (asm stubs) ─────────────────────────────────── */
void     cpu_halt(void) NORETURN;
void     cpu_enable_interrupts(void);
void     cpu_disable_interrupts(void);
uint32_t cpu_read_eflags(void);
uint32_t cpu_read_cr2(void);
uint32_t cpu_read_cr3(void);
void     cpu_write_cr3(uint32_t addr);
void     cpu_enable_paging(void);
void     cpu_disable_paging(void);

/* ═══════════════════════════════════════════════════════════════
 * VGA / Screen
 * ═══════════════════════════════════════════════════════════════ */
#define VGA_BASE        ((uint16_t*)0xB8000)
#define VGA_COLS        80
#define VGA_ROWS        25
#define VGA_TAB_WIDTH   8

typedef enum {
    COLOR_BLACK = 0, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN,
    COLOR_RED,       COLOR_MAGENTA, COLOR_BROWN, COLOR_LIGHT_GREY,
    COLOR_DARK_GREY, COLOR_LIGHT_BLUE, COLOR_LIGHT_GREEN, COLOR_LIGHT_CYAN,
    COLOR_LIGHT_RED, COLOR_LIGHT_MAGENTA, COLOR_YELLOW, COLOR_WHITE
} vga_color_t;

void screen_init(void);
void screen_clear(void);
void screen_set_color(vga_color_t fg, vga_color_t bg);
void screen_put_char(char c);
void screen_put_str(const char *s);
void screen_put_hex(uint32_t val);
void screen_put_dec(uint32_t val);
void screen_put_int(int32_t val);
void kprintf(const char *fmt, ...);
void screen_move_cursor(uint8_t col, uint8_t row);
void screen_get_cursor(uint8_t *col, uint8_t *row);
void screen_scroll(void);
void screen_set_attr(uint8_t col, uint8_t row, uint8_t attr);
void panic(const char *msg, const char *file, uint32_t line) NORETURN;

#define PANIC(msg)  panic((msg), __FILE__, __LINE__)
#define ASSERT(c)   do { if (UNLIKELY(!(c))) PANIC("Assertion failed: " #c); } while(0)

/* ═══════════════════════════════════════════════════════════════
 * GDT
 * ═══════════════════════════════════════════════════════════════ */
#define GDT_NULL_SEG    0x00
#define GDT_CODE_SEG    0x08
#define GDT_DATA_SEG    0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS_SEG     0x28
#define GDT_ENTRIES     7

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} PACKED gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} PACKED gdt_ptr_t;

void gdt_install(void);
void gdt_set_gate(int num, uint32_t base, uint32_t limit,
                  uint8_t access, uint8_t gran);

/* ═══════════════════════════════════════════════════════════════
 * IDT
 * ═══════════════════════════════════════════════════════════════ */
#define IDT_ENTRIES     256

typedef struct {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} PACKED idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} PACKED idt_ptr_t;

/* Saved registers pushed by ISR stub */
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; /* pusha */
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;                  /* CPU */
} PACKED registers_t;

typedef void (*isr_t)(registers_t *);

void idt_install(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void isr_handler(registers_t *regs);
void irq_install_handler(int irq, isr_t handler);
void irq_uninstall_handler(int irq);

/* ═══════════════════════════════════════════════════════════════
 * PIC (8259A)
 * ═══════════════════════════════════════════════════════════════ */
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1
#define PIC_EOI     0x20

void pic_remap(int offset1, int offset2);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);
uint16_t pic_get_irr(void);
uint16_t pic_get_isr(void);

/* ═══════════════════════════════════════════════════════════════
 * PIT (timer)
 * ═══════════════════════════════════════════════════════════════ */
#define PIT_BASE_FREQ   1193182UL

void     pit_install(uint32_t hz);
uint64_t pit_get_ticks(void);
void     pit_sleep_ms(uint32_t ms);

/* ═══════════════════════════════════════════════════════════════
 * Keyboard
 * ═══════════════════════════════════════════════════════════════ */
void keyboard_install(void);
int  keyboard_getchar(void);     /* blocking */
int  keyboard_poll(void);        /* non-blocking, -1 if empty */

/* ═══════════════════════════════════════════════════════════════
 * Memory / Physical Memory Manager
 * ═══════════════════════════════════════════════════════════════ */
#define PAGE_SIZE       4096
#define PAGE_PRESENT    BIT(0)
#define PAGE_WRITABLE   BIT(1)
#define PAGE_USER       BIT(2)
#define PAGE_PWT        BIT(3)
#define PAGE_PCD        BIT(4)
#define PAGE_ACCESSED   BIT(5)
#define PAGE_DIRTY      BIT(6)
#define PAGE_HUGE       BIT(7)

/* Multiboot2 memory-map entry */
typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;          /* 1 = usable */
    uint32_t reserved;
} PACKED mb2_mmap_entry_t;

/* PMM stats */
typedef struct {
    uint32_t total_frames;
    uint32_t used_frames;
    uint32_t free_frames;
} pmm_stats_t;

void     pmm_init(uint32_t mem_kb, uintptr_t kernel_end);
void     pmm_init_region(uintptr_t base, size_t len);
void     pmm_deinit_region(uintptr_t base, size_t len);
void    *pmm_alloc_frame(void);
void     pmm_free_frame(void *frame);
void     pmm_get_stats(pmm_stats_t *s);

/* ── Virtual Memory / Paging ─────────────────────────────────── */
typedef uint32_t  pte_t;
typedef uint32_t  pde_t;
typedef pde_t    *page_directory_t;

page_directory_t vmm_create_directory(void);
void             vmm_destroy_directory(page_directory_t dir);
void             vmm_map_page(page_directory_t dir, uintptr_t virt,
                              uintptr_t phys, uint32_t flags);
void             vmm_unmap_page(page_directory_t dir, uintptr_t virt);
uintptr_t        vmm_get_phys(page_directory_t dir, uintptr_t virt);
void             vmm_switch_directory(page_directory_t dir);
void             vmm_flush_tlb(uintptr_t virt);
void             vmm_init(void);

/* ── Heap allocator ─────────────────────────────────────────── */
void  kmalloc_init(uintptr_t start, size_t size);
void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size, size_t align);
void *kcalloc(size_t n, size_t size);
void *krealloc(void *ptr, size_t size);
void  kfree(void *ptr);
void  kmalloc_stats(size_t *used, size_t *free_bytes);

/* ═══════════════════════════════════════════════════════════════
 * String / memory utilities
 * ═══════════════════════════════════════════════════════════════ */
void  *kmemset (void *dst, int c, size_t n);
void  *kmemcpy (void *dst, const void *src, size_t n);
void  *kmemmove(void *dst, const void *src, size_t n);
int    kmemcmp (const void *a, const void *b, size_t n);
size_t kstrlen (const char *s);
int    kstrcmp (const char *a, const char *b);
int    kstrncmp(const char *a, const char *b, size_t n);
char  *kstrcpy (char *dst, const char *src);
char  *kstrncpy(char *dst, const char *src, size_t n);
char  *kstrcat (char *dst, const char *src);
char  *kstrchr (const char *s, int c);
char  *kstrrchr(const char *s, int c);
int    katoi   (const char *s);
void   kitoa   (int32_t val, char *buf, int base);
void   kutoa   (uint32_t val, char *buf, int base);

/* ═══════════════════════════════════════════════════════════════
 * Kernel main + subsystem init
 * ═══════════════════════════════════════════════════════════════ */
/* Multiboot2 info structure (simplified) */
typedef struct {
    uint32_t total_size;
    uint32_t reserved;
    /* followed by tags */
} PACKED mb2_info_t;

void kernel_main(mb2_info_t *mb_info);

/* Subsystem initialisation */
void gdt_install(void);
void idt_install(void);
void pic_remap(int o1, int o2);
void pit_install(uint32_t hz);
void keyboard_install(void);
void vmm_init(void);

/* ── Kernel log levels ───────────────────────────────────────── */
#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARN    2
#define LOG_ERROR   3

void klog(int level, const char *fmt, ...);

#endif /* KERNEL_H */
