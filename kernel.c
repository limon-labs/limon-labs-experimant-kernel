/* =============================================================
 * kernel/kernel.c  —  Kernel entry point & subsystem init
 * Called by boot.asm after GDT reload.
 * ============================================================= */
#include "../include/kernel.h"

/* ── Forward declarations for local helpers ─────────────────── */
static void parse_multiboot2(mb2_info_t *info,
                             uint32_t   *mem_kb_out,
                             uintptr_t  *kernel_end_out);
static void kernel_shell(void);
static void print_banner(void);
static void print_memory_info(void);

/* ── Kernel heap region (placed just after BSS by linker) ───── */
extern uint8_t __kernel_end[];          /* symbol from linker.ld */
#define HEAP_START  ALIGN_UP((uintptr_t)__kernel_end, PAGE_SIZE)
#define HEAP_SIZE   MB(4)

/* ── Multiboot2 tag types we care about ─────────────────────── */
#define MB2_TAG_END         0
#define MB2_TAG_CMDLINE     1
#define MB2_TAG_MEMINFO     4
#define MB2_TAG_MMAP        6
#define MB2_TAG_FRAMEBUFFER 8

typedef struct {
    uint32_t type;
    uint32_t size;
} PACKED mb2_tag_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t mem_lower;     /* KB below 1 MB */
    uint32_t mem_upper;     /* KB above 1 MB */
} PACKED mb2_tag_meminfo_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint64_t entry_addr;
    uint32_t entry_size;
    uint32_t entry_version;
    /* mb2_mmap_entry_t entries[] follow */
} PACKED mb2_tag_mmap_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    char     string[0];
} PACKED mb2_tag_cmdline_t;

/* ── Global kernel state ─────────────────────────────────────── */
static char    cmdline[256];
static uint32_t total_mem_kb  = 0;
static uint32_t kernel_end_pa = 0;

/* ═══════════════════════════════════════════════════════════════
 * kernel_main
 * Called from boot.asm with pointer to Multiboot2 info struct.
 * ═══════════════════════════════════════════════════════════════ */
void kernel_main(mb2_info_t *mb_info)
{
    /* ── 1. Screen (must come first so we can print) ──────── */
    screen_init();
    screen_clear();
    screen_set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print_banner();

    /* ── 2. Parse Multiboot2 tags ─────────────────────────── */
    screen_set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    kprintf("[BOOT] Parsing Multiboot2 info @ 0x%x\n", (uint32_t)mb_info);
    parse_multiboot2(mb_info, &total_mem_kb, &kernel_end_pa);
    kprintf("[BOOT] Memory: %u KB total  |  Kernel ends @ 0x%x\n",
            total_mem_kb, kernel_end_pa);
    if (cmdline[0])
        kprintf("[BOOT] Cmdline: %s\n", cmdline);

    /* ── 3. GDT ───────────────────────────────────────────── */
    screen_set_color(COLOR_YELLOW, COLOR_BLACK);
    kprintf("[GDT ] Installing Global Descriptor Table...\n");
    gdt_install();
    kprintf("[GDT ] OK — 6 descriptors (null, kcode, kdata, ucode, udata, tss)\n");

    /* ── 4. IDT ───────────────────────────────────────────── */
    kprintf("[IDT ] Installing Interrupt Descriptor Table...\n");
    idt_install();
    kprintf("[IDT ] OK — 256 gates set\n");

    /* ── 5. PIC ───────────────────────────────────────────── */
    kprintf("[PIC ] Remapping 8259A PIC (IRQ0=32, IRQ8=40)...\n");
    pic_remap(32, 40);
    kprintf("[PIC ] OK\n");

    /* ── 6. PIT ───────────────────────────────────────────── */
    kprintf("[PIT ] Installing timer @ 1000 Hz...\n");
    pit_install(1000);
    kprintf("[PIT ] OK\n");

    /* ── 7. Physical memory manager ───────────────────────── */
    kprintf("[PMM ] Initialising physical memory manager...\n");
    pmm_init(total_mem_kb, kernel_end_pa);
    pmm_stats_t pst;
    pmm_get_stats(&pst);
    kprintf("[PMM ] Frames: total=%u  used=%u  free=%u\n",
            pst.total_frames, pst.used_frames, pst.free_frames);

    /* ── 8. Virtual memory / paging ───────────────────────── */
    kprintf("[VMM ] Setting up paging...\n");
    vmm_init();
    kprintf("[VMM ] Paging enabled\n");

    /* ── 9. Kernel heap ───────────────────────────────────── */
    kprintf("[HEAP] Init @ 0x%x  size=%u KB\n",
            HEAP_START, HEAP_SIZE / 1024);
    kmalloc_init(HEAP_START, HEAP_SIZE);

    /* ── 10. Keyboard ─────────────────────────────────────── */
    kprintf("[KBD ] Installing PS/2 keyboard driver...\n");
    keyboard_install();
    kprintf("[KBD ] OK\n");

    /* ── 11. Enable interrupts ────────────────────────────── */
    kprintf("[CPU ] Enabling interrupts (STI)\n");
    cpu_enable_interrupts();

    /* ── 12. Print system summary ─────────────────────────── */
    screen_set_color(COLOR_WHITE, COLOR_BLACK);
    print_memory_info();

    /* ── 13. Drop into interactive shell ──────────────────── */
    screen_set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    kprintf("\nKernel initialisation complete.  Type 'help' for commands.\n\n");
    kernel_shell();

    /* Should never reach here */
    PANIC("kernel_main returned unexpectedly");
}

/* ═══════════════════════════════════════════════════════════════
 * Multiboot2 parser
 * ═══════════════════════════════════════════════════════════════ */
static void parse_multiboot2(mb2_info_t *info,
                             uint32_t   *mem_kb_out,
                             uintptr_t  *kernel_end_out)
{
    *mem_kb_out      = 0;
    *kernel_end_out  = (uintptr_t)info + info->total_size;

    mb2_tag_t *tag = (mb2_tag_t *)((uintptr_t)info + 8);

    while (tag->type != MB2_TAG_END) {
        switch (tag->type) {
        case MB2_TAG_MEMINFO: {
            mb2_tag_meminfo_t *m = (mb2_tag_meminfo_t *)tag;
            /* mem_upper is in KB above 1 MB */
            *mem_kb_out = m->mem_lower + m->mem_upper;
            break;
        }
        case MB2_TAG_MMAP: {
            mb2_tag_mmap_t *mm = (mb2_tag_mmap_t *)tag;
            mb2_mmap_entry_t *e =
                (mb2_mmap_entry_t *)((uintptr_t)mm + sizeof(*mm));
            mb2_mmap_entry_t *end =
                (mb2_mmap_entry_t *)((uintptr_t)mm + mm->size);
            while (e < end) {
                if (e->type == 1)          /* usable RAM */
                    pmm_init_region((uintptr_t)e->base_addr,
                                    (size_t)e->length);
                e = (mb2_mmap_entry_t *)((uintptr_t)e + mm->entry_size);
            }
            break;
        }
        case MB2_TAG_CMDLINE: {
            mb2_tag_cmdline_t *c = (mb2_tag_cmdline_t *)tag;
            kstrncpy(cmdline, c->string, sizeof(cmdline) - 1);
            break;
        }
        default:
            break;
        }
        /* Tags are 8-byte aligned */
        uintptr_t next = (uintptr_t)tag + tag->size;
        next = ALIGN_UP(next, 8);
        tag = (mb2_tag_t *)next;
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Minimal interactive shell
 * ═══════════════════════════════════════════════════════════════ */
#define SHELL_BUFLEN 128

static void shell_cmd_help(void);
static void shell_cmd_meminfo(void);
static void shell_cmd_clear(void);
static void shell_cmd_reboot(void);
static void shell_cmd_echo(const char *args);
static void shell_cmd_hexdump(const char *args);

typedef struct {
    const char *name;
    const char *desc;
    void (*fn_noarg)(void);
    void (*fn_arg)(const char *);
} shell_cmd_t;

static const shell_cmd_t cmds[] = {
    { "help",    "Show this help",              shell_cmd_help,    NULL              },
    { "meminfo", "Physical memory statistics",  shell_cmd_meminfo, NULL              },
    { "clear",   "Clear screen",               shell_cmd_clear,   NULL              },
    { "reboot",  "Reboot the machine",         shell_cmd_reboot,  NULL              },
    { "echo",    "Echo text",                  NULL,              shell_cmd_echo    },
    { "hexdump", "Dump memory: hexdump <addr> <len>", NULL,       shell_cmd_hexdump },
};
#define NUM_CMDS ARRAY_SIZE(cmds)

static void kernel_shell(void)
{
    char buf[SHELL_BUFLEN];
    uint32_t pos;

    while (TRUE) {
        screen_set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        kprintf("kernel> ");
        screen_set_color(COLOR_WHITE, COLOR_BLACK);

        /* Read line */
        pos = 0;
        while (TRUE) {
            int c = keyboard_getchar();
            if (c == '\n' || c == '\r') {
                buf[pos] = '\0';
                screen_put_char('\n');
                break;
            } else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    screen_put_char('\b');
                    screen_put_char(' ');
                    screen_put_char('\b');
                }
            } else if (c >= 32 && pos < SHELL_BUFLEN - 1) {
                buf[pos++] = (char)c;
                screen_put_char((char)c);
            }
        }

        if (pos == 0) continue;

        /* Split command from args */
        char *args = NULL;
        for (uint32_t i = 0; i < pos; i++) {
            if (buf[i] == ' ') {
                buf[i] = '\0';
                args = &buf[i + 1];
                break;
            }
        }

        /* Dispatch */
        bool found = FALSE;
        for (uint32_t i = 0; i < NUM_CMDS; i++) {
            if (kstrcmp(buf, cmds[i].name) == 0) {
                found = TRUE;
                if (cmds[i].fn_noarg) cmds[i].fn_noarg();
                else if (cmds[i].fn_arg) cmds[i].fn_arg(args ? args : "");
                break;
            }
        }
        if (!found) {
            screen_set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            kprintf("Unknown command: '%s'. Type 'help'.\n", buf);
            screen_set_color(COLOR_WHITE, COLOR_BLACK);
        }
    }
}

static void shell_cmd_help(void)
{
    screen_set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    kprintf("\nAvailable commands:\n");
    screen_set_color(COLOR_WHITE, COLOR_BLACK);
    for (uint32_t i = 0; i < NUM_CMDS; i++)
        kprintf("  %-12s  %s\n", cmds[i].name, cmds[i].desc);
    screen_put_char('\n');
}

static void shell_cmd_meminfo(void)
{
    pmm_stats_t s;
    pmm_get_stats(&s);
    size_t used, free_bytes;
    kmalloc_stats(&used, &free_bytes);
    screen_set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    kprintf("\n--- Physical Memory ---\n");
    screen_set_color(COLOR_WHITE, COLOR_BLACK);
    kprintf("  Total frames : %u  (%u KB)\n", s.total_frames,
            (s.total_frames * PAGE_SIZE) / 1024);
    kprintf("  Used  frames : %u  (%u KB)\n", s.used_frames,
            (s.used_frames  * PAGE_SIZE) / 1024);
    kprintf("  Free  frames : %u  (%u KB)\n", s.free_frames,
            (s.free_frames  * PAGE_SIZE) / 1024);
    screen_set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    kprintf("--- Kernel Heap ---\n");
    screen_set_color(COLOR_WHITE, COLOR_BLACK);
    kprintf("  Used  : %u bytes\n", (uint32_t)used);
    kprintf("  Free  : %u bytes\n", (uint32_t)free_bytes);
    screen_put_char('\n');
}

static void shell_cmd_clear(void)  { screen_clear(); }

static void shell_cmd_reboot(void)
{
    kprintf("Rebooting...\n");
    cpu_disable_interrupts();
    /* Pulse keyboard controller reset line */
    uint8_t tmp;
    do { tmp = inb(0x64); } while (tmp & 0x02);
    outb(0x64, 0xFE);
    cpu_halt();
}

static void shell_cmd_echo(const char *args)
{
    kprintf("%s\n", args);
}

static void shell_cmd_hexdump(const char *args)
{
    /* Parse: hexdump <hex_addr> <dec_len> */
    if (!args || !args[0]) {
        kprintf("Usage: hexdump <addr_hex> <len>\n");
        return;
    }
    /* Simple hex parser */
    uintptr_t addr = 0;
    const char *p = args;
    if (p[0]=='0' && p[1]=='x') p += 2;
    while (*p && *p != ' ') {
        char c = *p++;
        addr <<= 4;
        if (c >= '0' && c <= '9') addr |= c - '0';
        else if (c >= 'a' && c <= 'f') addr |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') addr |= c - 'A' + 10;
    }
    while (*p == ' ') p++;
    uint32_t len = (uint32_t)katoi(p);
    if (len == 0) len = 64;
    if (len > 512) len = 512;

    uint8_t *ptr = (uint8_t *)addr;
    for (uint32_t i = 0; i < len; i += 16) {
        kprintf("  %x: ", addr + i);
        for (uint32_t j = 0; j < 16 && (i+j) < len; j++) {
            uint8_t b = ptr[i+j];
            kprintf("%x%x ", b >> 4, b & 0xF);
        }
        kprintf(" |");
        for (uint32_t j = 0; j < 16 && (i+j) < len; j++) {
            char c = (char)ptr[i+j];
            screen_put_char(c >= 32 && c < 127 ? c : '.');
        }
        kprintf("|\n");
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Banner & info printers
 * ═══════════════════════════════════════════════════════════════ */
static void print_banner(void)
{
    kprintf("##############################################\n");
    kprintf("#                                            #\n");
    kprintf("#          MyOS Kernel  v0.1.0              #\n");
    kprintf("#      x86 Protected Mode  |  32-bit        #\n");
    kprintf("#                                            #\n");
    kprintf("##############################################\n\n");
}

static void print_memory_info(void)
{
    pmm_stats_t s;
    pmm_get_stats(&s);
    kprintf("Memory: %u MB total  |  %u MB free\n",
            (s.total_frames * PAGE_SIZE) / MB(1),
            (s.free_frames  * PAGE_SIZE) / MB(1));
}

/* ═══════════════════════════════════════════════════════════════
 * klog  — levelled kernel logger
 * ═══════════════════════════════════════════════════════════════ */
static const char *level_str[] = { "DEBUG", "INFO ", "WARN ", "ERROR" };
static const vga_color_t level_color[] = {
    COLOR_DARK_GREY, COLOR_LIGHT_CYAN, COLOR_YELLOW, COLOR_LIGHT_RED
};

void klog(int level, const char *fmt, ...)
{
    if (level < LOG_DEBUG || level > LOG_ERROR) return;
    screen_set_color(level_color[level], COLOR_BLACK);
    kprintf("[%s] ", level_str[level]);
    /* kprintf handles the va_list internally via screen.c */
    screen_set_color(COLOR_WHITE, COLOR_BLACK);
    /* For a real implementation pass va_list through — simplified here */
    kprintf(fmt);
}

/* ═══════════════════════════════════════════════════════════════
 * panic  — unrecoverable kernel error
 * ═══════════════════════════════════════════════════════════════ */
void panic(const char *msg, const char *file, uint32_t line)
{
    cpu_disable_interrupts();
    screen_set_color(COLOR_WHITE, COLOR_RED);
    screen_clear();
    kprintf("\n\n  *** KERNEL PANIC ***\n\n");
    kprintf("  Message : %s\n", msg);
    kprintf("  File    : %s\n", file);
    kprintf("  Line    : %u\n", line);
    kprintf("\n  System halted.\n");
    cpu_halt();
}
