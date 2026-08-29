#include "kernel.h"

void ahk_panic(const char *msg, const char *file, uint32_t line)
{
    cpu_disable_interrupts();
    screen_clear();
    
    kprintf("\n\n");
    kprintf("  ======================================\n");
    kprintf("  |        AHK KERNEL PANIC           |\n");
    kprintf("  ======================================\n\n");
    kprintf("  Message : %s\n", msg);
    kprintf("  File    : %s\n", file);
    kprintf("  Line    : %u\n", line);
    kprintf("\n  System halted.\n");
    
    while(1) {
        __asm__ volatile("cli; hlt");
    }
}
