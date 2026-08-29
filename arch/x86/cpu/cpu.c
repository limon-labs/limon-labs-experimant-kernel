#include "kernel.h"

void cpu_enable_interrupts(void)
{
    __asm__ volatile("sti");
}

void cpu_disable_interrupts(void)
{
    __asm__ volatile("cli");
}

void cpu_halt(void)
{
    __asm__ volatile("cli; hlt");
    while(1);
}

uint32_t cpu_read_eflags(void)
{
    uint32_t flags;
    __asm__ volatile("pushfl; popl %0" : "=r"(flags));
    return flags;
}

uint32_t cpu_read_cr2(void)
{
    uint32_t val;
    __asm__ volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

uint32_t cpu_read_cr3(void)
{
    uint32_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

void cpu_write_cr3(uint32_t addr)
{
    __asm__ volatile("mov %0, %%cr3" :: "r"(addr) : "memory");
}

void cpu_enable_paging(void)
{
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
}

void cpu_disable_paging(void)
{
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~0x80000000;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
}

void cpu_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
               uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf));
}
