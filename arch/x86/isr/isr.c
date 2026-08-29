#include "kernel.h"

static const char *exception_msgs[] = {
    "Division by Zero", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound Range", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Overrun", "Invalid TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 FPU Error", "Alignment Check", "Machine Check", "SIMD FP Exception",
    "Virtualization", "Control Protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Security Exception", "Reserved"
};

void isr_handler(registers_t *regs) {
    if (regs->int_no < 32) {
        if (regs->int_no == 14) {
            cis_handle_fault(regs);
            return;
        }
        kprintf("\n*** CPU EXCEPTION #%u ***\n", regs->int_no);
        kprintf("  %s\n", exception_msgs[regs->int_no]);
        kprintf("  EIP=0x%x CS=0x%x EFLAGS=0x%x\n",
                regs->eip, regs->cs, regs->eflags);
        kprintf("  EAX=0x%x EBX=0x%x ECX=0x%x EDX=0x%x\n",
                regs->eax, regs->ebx, regs->ecx, regs->edx);
        PANIC("Unhandled CPU exception");
    } else if (regs->int_no >= 32 && regs->int_no < 48) {
        uint8_t irq = (uint8_t)(regs->int_no - 32);
        isr_t handler = irq_get_handler(irq);
        if (handler) handler(regs);
        pic_send_eoi(irq);
    }
}
