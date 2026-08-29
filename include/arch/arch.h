#ifndef AHK_ARCH_H
#define AHK_ARCH_H

#include "../kernel/kernel.h"

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} PACKED registers_t;

typedef void (*isr_t)(registers_t *);

void gdt_install(void);
void idt_install(void);
void isr_handler(registers_t *regs);

#endif /* AHK_ARCH_H */
