#ifndef AHK_KERNEL_H
#define AHK_KERNEL_H

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

#define NULL            ((void*)0)
#define TRUE            1
#define FALSE           0
#define PACKED          __attribute__((packed))
#define NORETURN        __attribute__((noreturn))
#define ALIGNED(n)      __attribute__((aligned(n)))
#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))
#define ALIGN_UP(x,a)   (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x,a) ((x) & ~((a) - 1))
#define BIT(n)          (1U << (n))
#define KB(n)           ((n) * 1024U)
#define MB(n)           ((n) * 1024U * 1024U)
#define GB(n)           ((n) * 1024U * 1024U * 1024U)

#define ASSERT(c) do { if (!(c)) ahk_panic("Assertion failed: " #c, __FILE__, __LINE__); } while(0)
#define ASSERT_MSG(c, msg) do { if (!(c)) ahk_panic((msg), __FILE__, __LINE__); } while(0)

#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARN    2
#define LOG_ERROR   3
#define LOG_FATAL   4

/* ── Serial Functions ───────────────────────────────────────── */
void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);

/* ── String & Memory Functions ───────────────────────────────── */
size_t kstrlen(const char *s);
int    kstrcmp(const char *a, const char *b);
int    kstrncmp(const char *a, const char *b, size_t n);
char  *kstrcpy(char *dst, const char *src);
char  *kstrncpy(char *dst, const char *src, size_t n);
void  *kmemset(void *dst, int c, size_t n);
void  *kmemcpy(void *dst, const void *src, size_t n);
void  *kmemmove(void *dst, const void *src, size_t n);
int    kmemcmp(const void *a, const void *b, size_t n);
char  *kstrcat(char *dst, const char *src);
char  *kstrchr(const char *s, int c);
int    katoi(const char *s);
void   kitoa(int32_t val, char *buf, int base);
void   kutoa(uint32_t val, char *buf, int base);

/* ── Kernel Logging API ─────────────────────────────────────── */
void klog(int level, const char *fmt, ...);

#define LOG_DEBUG_MSG(fmt, ...) klog(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO_MSG(fmt, ...)  klog(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN_MSG(fmt, ...)  klog(LOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR_MSG(fmt, ...) klog(LOG_ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL_MSG(fmt, ...) klog(LOG_FATAL, fmt, ##__VA_ARGS__)

/* ── Screen Functions ───────────────────────────────────────── */
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
void kprintf(const char *fmt, ...);
void screen_set_attr(uint8_t col, uint8_t row, uint8_t attr);
void screen_get_cursor(uint8_t *col, uint8_t *row);
void screen_move_cursor(uint8_t col, uint8_t row);

/* ── Keyboard Functions ─────────────────────────────────────── */
void keyboard_install(void);
void keyboard_handler(void);
int  keyboard_getchar(void);
int  keyboard_poll(void);

/* ── CPU & I/O Functions ─────────────────────────────────────── */
void     cpu_halt(void) NORETURN;
void     cpu_enable_interrupts(void);
void     cpu_disable_interrupts(void);
uint32_t cpu_read_eflags(void);
uint32_t cpu_read_cr2(void);
uint32_t cpu_read_cr3(void);
void     cpu_write_cr3(uint32_t addr);
void     cpu_enable_paging(void);
void     cpu_disable_paging(void);
void     cpu_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                   uint32_t *ecx, uint32_t *edx);
void     outb(uint16_t port, uint8_t val);
uint8_t  inb(uint16_t port);
void     outw(uint16_t port, uint16_t val);
uint16_t inw(uint16_t port);
void     outl(uint16_t port, uint32_t val);
uint32_t inl(uint16_t port);
void     io_wait(void);

/* ── Panic & Fault Handling ─────────────────────────────────── */
void ahk_panic(const char *msg, const char *file, uint32_t line) NORETURN;
#define PANIC(msg)  ahk_panic((msg), __FILE__, __LINE__)

/* ── Registers & ISR ─────────────────────────────────────────── */
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} PACKED registers_t;

typedef void (*isr_t)(registers_t *);

/* ── GDT API ─────────────────────────────────────────────────── */
void gdt_install(void);
void gdt_set_gate(int num, uint32_t base, uint32_t limit,
                  uint8_t access, uint8_t gran);

/* ── IDT API ─────────────────────────────────────────────────── */
void idt_install(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

/* ── ISR API ─────────────────────────────────────────────────── */
void isr_handler(registers_t *regs);

/* ── IRQ API ─────────────────────────────────────────────────── */
void irq_install_handler(int irq, isr_t handler);
void irq_uninstall_handler(int irq);
isr_t irq_get_handler(int irq);

/* ── PIC API ─────────────────────────────────────────────────── */
void pic_remap(int offset1, int offset2);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);
uint16_t pic_get_irr(void);
uint16_t pic_get_isr(void);

/* ── PIT API ─────────────────────────────────────────────────── */
void     pit_install(uint32_t hz);
uint64_t pit_get_ticks(void);
void     pit_sleep_ms(uint32_t ms);

/* ── Memory Management API ───────────────────────────────────── */
#define PAGE_SIZE       4096
#define PAGE_PRESENT    BIT(0)
#define PAGE_WRITABLE   BIT(1)
#define PAGE_USER       BIT(2)
#define PAGE_PWT        BIT(3)
#define PAGE_PCD        BIT(4)
#define PAGE_ACCESSED   BIT(5)
#define PAGE_DIRTY      BIT(6)
#define PAGE_HUGE       BIT(7)

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

/* ── Heap API ────────────────────────────────────────────────── */
void  kmalloc_init(uintptr_t start, size_t size);
void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size, size_t align);
void *kcalloc(size_t n, size_t size);
void *krealloc(void *ptr, size_t size);
void  kfree(void *ptr);
void  kmalloc_stats(size_t *used, size_t *free_bytes);

/* ── Scheduler API ───────────────────────────────────────────── */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_ZOMBIE
} task_state_t;

typedef struct task {
    uint32_t id;
    char     name[32];
    task_state_t state;
    uint32_t eip, esp, ebp;
    uint32_t cr3;
    uint32_t priority;
    struct task *next;
} task_t;

void scheduler_init(void);
void scheduler_add_task(task_t *task);
void scheduler_remove_task(uint32_t task_id);
void scheduler_yield(void);
void scheduler_sleep(uint32_t ms);
void scheduler_wakeup(uint32_t task_id);
task_t *scheduler_current(void);

/* ── Process API ─────────────────────────────────────────────── */
typedef struct process {
    uint32_t pid;
    char     name[64];
    uint32_t state;
    uint32_t parent_pid;
    uint32_t cr3;
    uint32_t heap_start;
    uint32_t heap_end;
    struct process *next;
} process_t;

void process_init(void);
process_t *process_create(const char *name, uint32_t parent_pid);
void process_destroy(uint32_t pid);
void process_block(uint32_t pid);
void process_unblock(uint32_t pid);
process_t *process_get(uint32_t pid);

/* ── Sync API ────────────────────────────────────────────────── */
typedef struct {
    uint32_t locked;
    uint32_t owner;
    uint32_t spin_count;
} spinlock_t;

typedef struct {
    uint32_t locked;
    uint32_t owner;
    uint32_t waiters;
} mutex_t;

typedef struct {
    uint32_t count;
    uint32_t max;
    uint32_t waiters;
} semaphore_t;

void spinlock_init(spinlock_t *lock);
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

void semaphore_init(semaphore_t *sem, uint32_t max);
void semaphore_wait(semaphore_t *sem);
void semaphore_signal(semaphore_t *sem);

/* ── IPC API ─────────────────────────────────────────────────── */
typedef struct {
    uint32_t type;
    uint32_t sender;
    uint32_t receiver;
    uint32_t size;
    void    *data;
} message_t;

typedef struct {
    message_t *buffer;
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint32_t count;
    spinlock_t lock;
} message_queue_t;

void ipc_init(void);
void ipc_send(message_t *msg);
void ipc_receive(message_t *msg);
void ipc_queue_init(message_queue_t *queue, uint32_t size);
void ipc_queue_push(message_queue_t *queue, message_t *msg);
int  ipc_queue_pop(message_queue_t *queue, message_t *msg);

/* ── Time API ────────────────────────────────────────────────── */
void     time_init(void);
uint64_t time_get_ticks(void);
uint32_t time_get_uptime_ms(void);
void     time_sleep_ms(uint32_t ms);
void     time_sleep_us(uint32_t us);

/* ── CIS API ─────────────────────────────────────────────────── */
typedef enum {
    FAULT_NORMAL,
    FAULT_WARNING,
    FAULT_CRITICAL,
    FAULT_FATAL
} fault_severity_t;

typedef enum {
    FAULT_PAGE_FAULT,
    FAULT_GPF,
    FAULT_DOUBLE_FAULT,
    FAULT_STACK_OVERFLOW,
    FAULT_HEAP_CORRUPTION,
    FAULT_DEADLOCK,
    FAULT_DIVIDE_BY_ZERO,
    FAULT_INVALID_OPCODE
} fault_type_t;

typedef struct {
    fault_type_t type;
    fault_severity_t severity;
    uint32_t fault_addr;
    uint32_t instruction_ptr;
    char     description[64];
} fault_t;

void cis_init(void);
void cis_handle_fault(registers_t *regs);
void cis_detect_fault(fault_t *fault);
void cis_diagnose_fault(fault_t *fault);
void cis_remediate(fault_t *fault);
void cis_verify_remediation(fault_t *fault);
void cis_learn(fault_type_t type, const char *solution);
bool cis_recall(fault_type_t type, char *out_solution);
void cis_get_stats(uint32_t *faults, uint32_t *recovered, uint32_t *escalated);

/* ── Kernel Main ─────────────────────────────────────────────── */
void ahk_kernel_main(void);

#endif /* AHK_KERNEL_H */
