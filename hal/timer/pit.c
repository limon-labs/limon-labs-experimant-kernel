#include "kernel.h"

#define PIT_CH0     0x40
#define PIT_CMD     0x43
#define PIT_MODE3   0x36
#define PIT_BASE_FREQ 1193182

static volatile uint64_t pit_ticks = 0;
static uint32_t pit_hz = 0;

static void pit_irq_handler(registers_t *regs) {
    (void)regs;
    pit_ticks++;
}

void pit_install(uint32_t hz) {
    pit_hz = hz;
    uint32_t divisor = PIT_BASE_FREQ / hz;
    outb(PIT_CMD, PIT_MODE3);
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
    irq_install_handler(0, pit_irq_handler);
    pic_clear_mask(0);
}

uint64_t pit_get_ticks(void) {
    return pit_ticks;
}

void pit_sleep_ms(uint32_t ms) {
    uint64_t target = pit_ticks + (uint64_t)(ms * pit_hz / 1000);
    while (pit_ticks < target)
        __asm__ volatile("hlt");
}
