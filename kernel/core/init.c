#include "kernel.h"

void ahk_init_system(void)
{
    /* 1. Initialize Screen */
    screen_init();
    screen_clear();
    
    /* 2. Initialize Serial (for debugging) */
    serial_init();
    
    /* 3. Initialize GDT */
    LOG_INFO_MSG("[BOOT] Initializing GDT...\n");
    gdt_install();
    
    /* 4. Initialize IDT */
    LOG_INFO_MSG("[BOOT] Initializing IDT...\n");
    idt_install();
    
    /* 5. Initialize PIC */
    LOG_INFO_MSG("[BOOT] Initializing PIC...\n");
    pic_remap(32, 40);
    
    /* 6. Initialize PIT */
    LOG_INFO_MSG("[BOOT] Initializing PIT...\n");
    pit_install(1000);
    
    /* 7. Initialize CIS */
    LOG_INFO_MSG("[BOOT] Initializing CIS...\n");
    cis_init();
    
    /* 8. Initialize Keyboard */
    LOG_INFO_MSG("[BOOT] Initializing Keyboard...\n");
    keyboard_install();
    
    /* 9. Enable Interrupts */
    LOG_INFO_MSG("[BOOT] Enabling Interrupts...\n");
    cpu_enable_interrupts();
    
    LOG_INFO_MSG("[BOOT] System Initialization Complete!\n");
}

void ahk_shell_start(void)
{
    char buf[128];
    uint32_t pos = 0;
    
    kprintf("Type 'help' for commands.\n\n");
    
    while (1) {
        kprintf("ahk> ");
        pos = 0;
        while (1) {
            int c = keyboard_getchar();
            if (c == '\n' || c == '\r') {
                buf[pos] = '\0';
                kprintf("\n");
                break;
            } else if (c == '\b') {
                if (pos > 0) pos--;
                kprintf("\b \b");
            } else if (c >= 32 && pos < 127) {
                buf[pos++] = (char)c;
                kprintf("%c", c);
            }
        }
        if (pos == 0) continue;
        
        if (kstrcmp(buf, "help") == 0) {
            kprintf("Commands: help, clear, reboot, echo <text>\n");
        } else if (kstrcmp(buf, "clear") == 0) {
            screen_clear();
        } else if (kstrcmp(buf, "reboot") == 0) {
            cpu_disable_interrupts();
            outb(0x64, 0xFE);
            cpu_halt();
        } else if (kstrncmp(buf, "echo ", 5) == 0) {
            kprintf("%s\n", &buf[5]);
        } else {
            kprintf("Unknown command: %s\n", buf);
        }
    }
}
