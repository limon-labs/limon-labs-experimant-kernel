#include "kernel.h"

static uint16_t *vga_buffer = (uint16_t *)0xB8000;
static uint8_t   cursor_col = 0;
static uint8_t   cursor_row = 0;
static uint8_t   cur_fg     = COLOR_LIGHT_GREY;
static uint8_t   cur_bg     = COLOR_BLACK;

#define VGA_CTRL_REG    0x3D4
#define VGA_DATA_REG    0x3D5
#define VGA_CURSOR_HI   0x0E
#define VGA_CURSOR_LO   0x0F

/* Forward declaration */
static void screen_scroll(void);

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
    uint16_t pos = (uint16_t)(cursor_row * 80 + cursor_col);
    outb(VGA_CTRL_REG, VGA_CURSOR_HI);
    outb(VGA_DATA_REG, (uint8_t)(pos >> 8));
    outb(VGA_CTRL_REG, VGA_CURSOR_LO);
    outb(VGA_DATA_REG, (uint8_t)(pos & 0xFF));
}

void screen_init(void)
{
    vga_buffer = (uint16_t *)0xB8000;
    cursor_col = 0;
    cursor_row = 0;
    screen_clear();
}

void screen_clear(void)
{
    uint8_t attr = make_attr((vga_color_t)cur_fg, (vga_color_t)cur_bg);
    uint16_t blank = make_entry(' ', attr);
    for (int i = 0; i < 80 * 25; i++)
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

void screen_put_char(char c)
{
    uint8_t attr = make_attr((vga_color_t)cur_fg, (vga_color_t)cur_bg);
    switch (c) {
    case '\n':
        cursor_col = 0; cursor_row++; break;
    case '\r':
        cursor_col = 0; break;
    case '\b':
        if (cursor_col > 0) {
            cursor_col--;
            vga_buffer[cursor_row*80 + cursor_col] = make_entry(' ', attr);
        }
        break;
    default:
        if (c >= 32) {
            vga_buffer[cursor_row*80 + cursor_col] = make_entry(c, attr);
            cursor_col++;
        }
        break;
    }
    if (cursor_col >= 80) { cursor_col = 0; cursor_row++; }
    while (cursor_row >= 25) screen_scroll();
    hw_cursor_update();
}

void screen_put_str(const char *s)
{
    while (*s) screen_put_char(*s++);
}

static void screen_scroll(void)
{
    uint8_t attr = make_attr((vga_color_t)cur_fg, (vga_color_t)cur_bg);
    uint16_t blank = make_entry(' ', attr);
    for (int row = 1; row < 25; row++)
        for (int col = 0; col < 80; col++)
            vga_buffer[(row-1)*80 + col] = vga_buffer[row*80 + col];
    for (int col = 0; col < 80; col++)
        vga_buffer[24*80 + col] = blank;
    if (cursor_row > 0) cursor_row--;
}

void kprintf(const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    char buf[32];
    while (*fmt) {
        if (*fmt != '%') { screen_put_char(*fmt++); continue; }
        fmt++;
        switch (*fmt++) {
        case 'c': screen_put_char((char)__builtin_va_arg(args, int)); break;
        case 's': screen_put_str(__builtin_va_arg(args, const char *)); break;
        case 'd': {
            int32_t v = __builtin_va_arg(args, int32_t);
            if (v < 0) { screen_put_char('-'); v = -v; }
            kutoa((uint32_t)v, buf, 10);
            screen_put_str(buf);
            break;
        }
        case 'u': kutoa(__builtin_va_arg(args, uint32_t), buf, 10); screen_put_str(buf); break;
        case 'x': kutoa(__builtin_va_arg(args, uint32_t), buf, 16); screen_put_str(buf); break;
        case 'p': kutoa((uint32_t)__builtin_va_arg(args, void *), buf, 16); screen_put_str(buf); break;
        case '%': screen_put_char('%'); break;
        default: screen_put_char('?'); break;
        }
    }
    __builtin_va_end(args);
}
