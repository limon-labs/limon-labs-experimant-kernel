#include "kernel.h"

#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1
#define PIC_EOI     0x20

void pic_remap(int offset1, int offset2) {
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();
    outb(PIC1_DATA, (uint8_t)offset1); io_wait();
    outb(PIC2_DATA, (uint8_t)offset2); io_wait();
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, (uint8_t)(inb(port) | BIT(irq)));
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, (uint8_t)(inb(port) & ~BIT(irq)));
}

uint16_t pic_get_irr(void) {
    outb(PIC1_CMD, 0x0A);
    outb(PIC2_CMD, 0x0A);
    return (uint16_t)((uint16_t)inb(PIC2_DATA) << 8 | inb(PIC1_DATA));
}

uint16_t pic_get_isr(void) {
    outb(PIC1_CMD, 0x0B);
    outb(PIC2_CMD, 0x0B);
    return (uint16_t)((uint16_t)inb(PIC2_DATA) << 8 | inb(PIC1_DATA));
}
