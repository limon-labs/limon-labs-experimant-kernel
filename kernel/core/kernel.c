#include "kernel.h"

/* Forward declarations */
void ahk_init_system(void);
void ahk_shell_start(void);

void ahk_kernel_main(void)
{
    /* System Initialization */
    ahk_init_system();
    
    /* Print Banner */
    kprintf("\n");
    kprintf("  ========================================\n");
    kprintf("  |     AHK OS - Production Grade       |\n");
    kprintf("  |     Self-Healing Kernel (CIS)       |\n");
    kprintf("  |     Version 1.0.0                   |\n");
    kprintf("  ========================================\n\n");

    /* Start Interactive Shell */
    ahk_shell_start();
}
