#include "kernel.h"

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} PACKED gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} PACKED gdt_ptr_t;

#define GDT_ENTRIES 7
#define GDT_CODE_SEG 0x08
#define GDT_DATA_SEG 0x10

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t gdt_ptr_val;

static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran) {
    gdt[i].base_low = (uint16_t)(base & 0xFFFF);
    gdt[i].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt[i].base_high = (uint8_t)((base >> 24) & 0xFF);
    gdt[i].limit_low = (uint16_t)(limit & 0xFFFF);
    gdt[i].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    gdt[i].access = access;
}

void gdt_install(void) {
    gdt_ptr_val.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr_val.base = (uint32_t)(uintptr_t)&gdt;
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);
    gdt_set_entry(5, 0, 0, 0, 0);
    gdt_set_entry(6, 0, 0, 0, 0);
    __asm__ volatile(
        "lgdt (%0)\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "ljmp $0x08, $.flush\n"
        ".flush:\n"
        :: "r"(&gdt_ptr_val) : "eax", "memory"
    );
}
