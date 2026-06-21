/* =============================================================
 * linker.ld  —  Kernel linker script
 *
 * Produces a 32-bit ELF suitable for loading by GRUB/Multiboot2.
 * Physical load address: 1 MB (0x00100000)
 * ============================================================= */

OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

SECTIONS
{
    /* ── Load kernel at the 1 MB mark ───────────────────────── */
    . = 0x00100000;

    /* ── Multiboot header must be within the first 32 KB ────── */
    .multiboot ALIGN(8) :
    {
        *(.multiboot)
    }

    /* ── Read-only text ─────────────────────────────────────── */
    .text ALIGN(4096) :
    {
        __kernel_start = .;
        *(.text)
        *(.text.*)
    }

    /* ── Read-only data ─────────────────────────────────────── */
    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* ── Initialised data ───────────────────────────────────── */
    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    }

    /* ── Uninitialised data (BSS) ────────────────────────────── */
    .bss ALIGN(4096) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        . = ALIGN(4096);
        __bss_end = .;
    }

    __kernel_end = .;

    /* ── Discard unwanted sections ───────────────────────────── */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.debug*)
        *(.gcc_except_table*)
    }
}
