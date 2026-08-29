#include "kernel.h"

static volatile uint64_t system_ticks = 0;
static uint32_t tick_hz = 1000;

void time_init(void) {
    system_ticks = 0;
    pit_install(tick_hz);
}

uint64_t time_get_ticks(void) {
    return system_ticks;
}

uint32_t time_get_uptime_ms(void) {
    return (uint32_t)(system_ticks * 1000 / tick_hz);
}

void time_sleep_ms(uint32_t ms) {
    uint64_t target = system_ticks + (uint64_t)(ms * tick_hz / 1000);
    while (system_ticks < target)
        __asm__ volatile("hlt");
}

void time_sleep_us(uint32_t us) {
    uint64_t target = system_ticks + (uint64_t)(us * tick_hz / 1000000);
    while (system_ticks < target)
        __asm__ volatile("hlt");
}
