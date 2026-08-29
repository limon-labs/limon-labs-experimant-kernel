/* =============================================================
 * kernel/screen.c  —  VGA text-mode driver + kprintf
 * 80×25 colour text mode, cursor control, scrolling, kprintf.
 * ============================================================= */
#include "../include/kernel.h"

/* ── VGA I/O ports ──────────────────────────────────────────── */
#define VGA_CTRL_REG    0x3D4
#define VGA_DATA_REG    0x3D5
#define VGA_CURSOR_HI   0x0E
#define VGA_CURSOR_LO   0x0F
#define VGA_CURSOR_START 0x0A
#define VGA_CURSOR_END   0x0B

/* ── Internal state ─────────────────────────────────────────── */
static uint16_t *vga_buffer = (uint16_t *)0xB8000;
static uint8_t   cursor_col = 0;
static uint8_t   cursor_row = 0;
static uint8_t   cur_fg     = COLOR_LIGHT_GREY;
static uint8_t   cur_bg     = COLOR_BLACK;

/* ── Helpers ─────────────────────────────────────────────────── */
static inline uint8_t make_attr(vga_color_t fg, vga_color_t bg)
{
    return (uint8_t)(((uint8_t)bg << 4) | ((uint8_t)fg & 0x0F));
}

static inline uint16_t make_entry(char c, uint8_t attr)
{
    return (uint16_t)((uint16_t)attr << 8 | (uint8_t)c);
}

static void hw_cursor_update(void)
{
    uint16_t pos = (uint16_t)(cursor_row * VGA_COLS + cursor_col);
    outb(VGA_CTRL_REG, VGA_CURSOR_HI);
    outb(VGA_DATA_REG, (uint8_t)(pos >> 8));
    outb(VGA_CTRL_REG, VGA_CURSOR_LO);
    outb(VGA_DATA_REG, (uint8_t)(pos & 0xFF));
}

static void hw_cursor_enable(uint8_t start, uint8_t end)
{
    outb(VGA_CTRL_REG, VGA_CURSOR_START);
    outb(VGA_DATA_REG, (uint8_t)((inb(VGA_DATA_REG) & 0xC0) | start));
    outb(VGA_CTRL_REG, VGA_CURSOR_END);
    outb(VGA_DATA_REG, (uint8_t)((inb(VGA_DATA_REG) & 0xE0) | end));
}

/* ═══════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════ */

void screen_init(void)
{
    vga_buffer = (uint16_t *)0xB8000;
    cursor_col = 0;
    cursor_row = 0;
    cur_fg     = COLOR_LIGHT_GREY;
    cur_bg     = COLOR_BLACK;
    hw_cursor_enable(14, 15);
    screen_clear();
}

void screen_clear(void)
{
    uint8_t attr = make_attr((vga_color_t)cur_fg, (vga_color_t)cur_bg);
    uint16_t blank = make_entry(' ', attr);
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        vga_buffer[i] = blank;
    cursor_col = 0;
    cursor_row = 0;
    hw_cursor_update();
}

void screen_set_color(vga_color_t fg, vga_color_t bg)
{
    cur_fg = (uint8_t)fg;
    cur_bg = (uint8_t)bg;
}

void screen_scroll(void)
{
    uint8_t attr = make_attr((vga_color_t)cur_fg, (vga_color_t)cur_bg);
    uint16_t blank = make_entry(' ', attr);

    /* Move all rows up by one */
    for (int row = 1; row < VGA_ROWS; row++)
        for (int col = 0; col < VGA_COLS; col++)
            vga_buffer[(row-1)*VGA_COLS + col] =
                vga_buffer[row*VGA_COLS + col];

    /* Clear last row */
    for (int col = 0; col < VGA_COLS; col++)
        vga_buffer[(VGA_ROWS-1)*VGA_COLS + col] = blank;

    if (cursor_row > 0) cursor_row--;
}

void screen_put_char(char c)
{
    uint8_t attr = make_attr((vga_color_t)cur_fg, (vga_color_t)cur_bg);

    switch (c) {
    case '\n':
        cursor_col = 0;
        cursor_row++;
        break;
    case '\r':
        cursor_col = 0;
        break;
    case '\t': {
        uint8_t next = (uint8_t)(ALIGN_UP(cursor_col + 1, VGA_TAB_WIDTH));
        if (next >= VGA_COLS) next = VGA_COLS - 1;
        while (cursor_col < next) {
            vga_buffer[cursor_row * VGA_COLS + cursor_col] =
                make_entry(' ', attr);
            cursor_col++;
        }
        break;
    }
    case '\b':
        if (cursor_col > 0) {
            cursor_col--;
            vga_buffer[cursor_row * VGA_COLS + cursor_col] =
                make_entry(' ', attr);
        }
        break;
    default:
        if (c >= 32) {
            vga_buffer[cursor_row * VGA_COLS + cursor_col] =
                make_entry(c, attr);
            cursor_col++;
        }
        break;
    }

    if (cursor_col >= VGA_COLS) {
        cursor_col = 0;
        cursor_row++;
    }
    while (cursor_row >= VGA_ROWS)
        screen_scroll();

    hw_cursor_update();
}

void screen_put_str(const char *s)
{
    while (*s) screen_put_char(*s++);
}

void screen_move_cursor(uint8_t col, uint8_t row)
{
    if (col < VGA_COLS && row < VGA_ROWS) {
        cursor_col = col;
        cursor_row = row;
        hw_cursor_update();
    }
}

void screen_get_cursor(uint8_t *col, uint8_t *row)
{
    if (col) *col = cursor_col;
    if (row) *row = cursor_row;
}

void screen_set_attr(uint8_t col, uint8_t row, uint8_t attr)
{
    if (col < VGA_COLS && row < VGA_ROWS) {
        uint16_t *p = &vga_buffer[row * VGA_COLS + col];
        *p = (uint16_t)(((uint16_t)attr << 8) | (*p & 0xFF));
    }
}

void screen_put_hex(uint32_t val)
{
    char buf[11];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 9; i >= 2; i--) {
        uint8_t nibble = (uint8_t)(val & 0xF);
        buf[i] = (char)(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
        val >>= 4;
    }
    buf[10] = '\0';
    screen_put_str(buf);
}

void screen_put_dec(uint32_t val)
{
    char buf[12];
    kutoa(val, buf, 10);
    screen_put_str(buf);
}

void screen_put_int(int32_t val)
{
    char buf[13];
    kitoa(val, buf, 10);
    screen_put_str(buf);
}

/* ═══════════════════════════════════════════════════════════════
 * kprintf  —  kernel printf (subset of printf)
 *
 * Supported format specifiers:
 *   %c  %s  %d  %i  %u  %x  %X  %p  %o  %b  %%
 *   Width: %8d, %-8s   Zero-pad: %08x
 * ═══════════════════════════════════════════════════════════════ */

/* va_list support — use GCC builtins */
typedef __builtin_va_list va_list;
#define va_start(v,l)   __builtin_va_start(v,l)
#define va_end(v)       __builtin_va_end(v)
#define va_arg(v,l)     __builtin_va_arg(v,l)

static void print_padded(const char *s, size_t len,
                         int width, bool left_align, char pad_char)
{
    int slen = (int)len;
    if (!left_align)
        for (int i = slen; i < width; i++)
            screen_put_char(pad_char);
    for (int i = 0; i < slen; i++)
        screen_put_char(s[i]);
    if (left_align)
        for (int i = slen; i < width; i++)
            screen_put_char(' ');
}

void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') { screen_put_char(*fmt++); continue; }
        fmt++;  /* skip '%' */

        /* Flags */
        bool left_align = FALSE;
        bool zero_pad   = FALSE;
        bool show_sign  = FALSE;
        bool prefix     = FALSE;

parse_flags:
        switch (*fmt) {
        case '-': left_align = TRUE; fmt++; goto parse_flags;
        case '0': zero_pad   = TRUE; fmt++; goto parse_flags;
        case '+': show_sign  = TRUE; fmt++; goto parse_flags;
        case '#': prefix     = TRUE; fmt++; goto parse_flags;
        default: break;
        }

        /* Width */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');

        char pad_char = (zero_pad && !left_align) ? '0' : ' ';

        /* Specifier */
        char buf[64];
        const char *str;
        size_t slen;

        switch (*fmt++) {
        case 'c': {
            char c = (char)va_arg(args, int);
            buf[0] = c; buf[1] = '\0';
            print_padded(buf, 1, width, left_align, pad_char);
            break;
        }
        case 's': {
            str = va_arg(args, const char *);
            if (!str) str = "(null)";
            slen = kstrlen(str);
            print_padded(str, slen, width, left_align, ' ');
            break;
        }
        case 'd':
        case 'i': {
            int32_t v = va_arg(args, int32_t);
            if (show_sign && v >= 0) { buf[0]='+'; kitoa(v, buf+1, 10); }
            else kitoa(v, buf, 10);
            slen = kstrlen(buf);
            print_padded(buf, slen, width, left_align, pad_char);
            break;
        }
        case 'u': {
            uint32_t v = va_arg(args, uint32_t);
            kutoa(v, buf, 10);
            slen = kstrlen(buf);
            print_padded(buf, slen, width, left_align, pad_char);
            break;
        }
        case 'x': {
            uint32_t v = va_arg(args, uint32_t);
            if (prefix) { buf[0]='0'; buf[1]='x'; kutoa(v, buf+2, 16); }
            else kutoa(v, buf, 16);
            slen = kstrlen(buf);
            print_padded(buf, slen, width, left_align, pad_char);
            break;
        }
        case 'X': {
            uint32_t v = va_arg(args, uint32_t);
            kutoa(v, buf, 16);
            /* uppercase */
            for (char *p = buf; *p; p++)
                if (*p >= 'a' && *p <= 'f') *p -= 32;
            slen = kstrlen(buf);
            print_padded(buf, slen, width, left_align, pad_char);
            break;
        }
        case 'o': {
            uint32_t v = va_arg(args, uint32_t);
            kutoa(v, buf, 8);
            slen = kstrlen(buf);
            print_padded(buf, slen, width, left_align, pad_char);
            break;
        }
        case 'b': {
            uint32_t v = va_arg(args, uint32_t);
            kutoa(v, buf, 2);
            slen = kstrlen(buf);
            print_padded(buf, slen, width, left_align, pad_char);
            break;
        }
        case 'p': {
            uint32_t v = (uint32_t)va_arg(args, void *);
            buf[0]='0'; buf[1]='x'; kutoa(v, buf+2, 16);
            slen = kstrlen(buf);
            print_padded(buf, slen, width < 10 ? 10 : width,
                         left_align, '0');
            break;
        }
        case '%':
            screen_put_char('%');
            break;
        case '\0':
            goto done;
        default:
            screen_put_char('?');
            break;
        }
    }
done:
    va_end(args);
}

/* ═══════════════════════════════════════════════════════════════
 * String / memory utility implementations
 * (kept in screen.c for link simplicity; could be split to util.c)
 * ═══════════════════════════════════════════════════════════════ */
void *kmemset(void *dst, int c, size_t n)
{
    uint8_t *p = (uint8_t *)dst;
    while (n--) *p++ = (uint8_t)c;
    return dst;
}

void *kmemcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *kmemmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s || d >= s + n) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int kmemcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *p = (const uint8_t *)a;
    const uint8_t *q = (const uint8_t *)b;
    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

size_t kstrlen(const char *s)
{
    size_t n = 0;
    while (*s++) n++;
    return n;
}

int kstrcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int kstrncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *kstrcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *kstrncpy(char *dst, const char *src, size_t n)
{
    char *d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dst;
}

char *kstrcat(char *dst, const char *src)
{
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++));
    return dst;
}

char *kstrchr(const char *s, int c)
{
    while (*s) { if (*s == (char)c) return (char *)s; s++; }
    return (c == 0) ? (char *)s : NULL;
}

char *kstrrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s) { if (*s == (char)c) last = s; s++; }
    return (char *)last;
}

int katoi(const char *s)
{
    int n = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (*s++ - '0');
    return n * sign;
}

void kitoa(int32_t val, char *buf, int base)
{
    char tmp[33];
    int  i = 0;
    bool neg = FALSE;

    if (val < 0 && base == 10) { neg = TRUE; val = -val; }
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }

    uint32_t uval = (uint32_t)val;
    while (uval) {
        uint32_t rem = uval % (uint32_t)base;
        tmp[i++] = (char)(rem < 10 ? '0' + rem : 'a' + rem - 10);
        uval /= (uint32_t)base;
    }
    if (neg) tmp[i++] = '-';
    tmp[i] = '\0';

    /* Reverse */
    for (int j = 0; j < i; j++) buf[j] = tmp[i-1-j];
    buf[i] = '\0';
}

void kutoa(uint32_t val, char *buf, int base)
{
    char tmp[33];
    int  i = 0;

    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (val) {
        uint32_t rem = val % (uint32_t)base;
        tmp[i++] = (char)(rem < 10 ? '0' + rem : 'a' + rem - 10);
        val /= (uint32_t)base;
    }
    for (int j = 0; j < i; j++) buf[j] = tmp[i-1-j];
    buf[i] = '\0';
}
