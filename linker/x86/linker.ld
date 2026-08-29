OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .text ALIGN(4096) :
    {
        __kernel_start = .;
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    }

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

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.debug*)
        *(.gcc_except_table*)
    }
}
