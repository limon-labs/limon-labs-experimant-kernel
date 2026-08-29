#include "kernel.h"

#define IRQ_HANDLERS_COUNT 16

static isr_t irq_handlers[IRQ_HANDLERS_COUNT];

void irq_install_handler(int irq, isr_t handler) {
    if (irq >= 0 && irq < IRQ_HANDLERS_COUNT)
        irq_handlers[irq] = handler;
}

void irq_uninstall_handler(int irq) {
    if (irq >= 0 && irq < IRQ_HANDLERS_COUNT)
        irq_handlers[irq] = NULL;
}

isr_t irq_get_handler(int irq) {
    if (irq >= 0 && irq < IRQ_HANDLERS_COUNT)
        return irq_handlers[irq];
    return NULL;
}
