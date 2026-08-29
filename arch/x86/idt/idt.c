#include "kernel.h"

typedef struct {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} PACKED idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} PACKED idt_ptr_t;

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_ptr_val;

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

void idt_set_gate(uint8_t n, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt[n].base_low  = (uint16_t)(base & 0xFFFF);
    idt[n].base_high = (uint16_t)((base >> 16) & 0xFFFF);
    idt[n].sel       = sel;
    idt[n].always0   = 0;
    idt[n].flags     = flags;
}

void idt_install(void)
{
    idt_ptr_val.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr_val.base  = (uint32_t)(uintptr_t)&idt;

    kmemset(&idt, 0, sizeof(idt));

#define SET_ISR(n) idt_set_gate(n, (uint32_t)isr##n, 0x08, 0x8E)
    SET_ISR(0);  SET_ISR(1);  SET_ISR(2);  SET_ISR(3);
    SET_ISR(4);  SET_ISR(5);  SET_ISR(6);  SET_ISR(7);
    SET_ISR(8);  SET_ISR(9);  SET_ISR(10); SET_ISR(11);
    SET_ISR(12); SET_ISR(13); SET_ISR(14); SET_ISR(15);
    SET_ISR(16); SET_ISR(17); SET_ISR(18); SET_ISR(19);
    SET_ISR(20); SET_ISR(21); SET_ISR(22); SET_ISR(23);
    SET_ISR(24); SET_ISR(25); SET_ISR(26); SET_ISR(27);
    SET_ISR(28); SET_ISR(29); SET_ISR(30); SET_ISR(31);
#undef SET_ISR

    pic_remap(32, 40);

#define SET_IRQ(n,v) idt_set_gate(v, (uint32_t)irq##n, 0x08, 0x8E)
    SET_IRQ(0,32);  SET_IRQ(1,33);  SET_IRQ(2,34);  SET_IRQ(3,35);
    SET_IRQ(4,36);  SET_IRQ(5,37);  SET_IRQ(6,38);  SET_IRQ(7,39);
    SET_IRQ(8,40);  SET_IRQ(9,41);  SET_IRQ(10,42); SET_IRQ(11,43);
    SET_IRQ(12,44); SET_IRQ(13,45); SET_IRQ(14,46); SET_IRQ(15,47);
#undef SET_IRQ

    __asm__ volatile("lidt (%0)" :: "r"(&idt_ptr_val) : "memory");
}
