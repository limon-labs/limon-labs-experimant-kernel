#include "kernel.h"

static const char *log_levels[] = {
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR",
    "FATAL"
};

static const vga_color_t log_colors[] = {
    COLOR_DARK_GREY,
    COLOR_LIGHT_GREEN,
    COLOR_YELLOW,
    COLOR_LIGHT_RED,
    COLOR_RED
};

void klog(int level, const char *fmt, ...)
{
    if (level < LOG_DEBUG || level > LOG_FATAL) return;
    
    screen_set_color(log_colors[level], COLOR_BLACK);
    kprintf("[%s] ", log_levels[level]);
    
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    // We'll implement a va_list version of kprintf in screen.c
    __builtin_va_end(args);
    
    kprintf(fmt);
    screen_set_color(COLOR_WHITE, COLOR_BLACK);
}
