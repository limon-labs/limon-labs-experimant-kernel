; ============================================================
;  boot.asm — Multiboot2-compliant bootloader entry point
;  Assembles with: nasm -f elf32 boot.asm -o boot.o
; ============================================================

bits 32

; ─── Multiboot2 Header Constants ────────────────────────────
MULTIBOOT2_MAGIC        equ 0xE85250D6
MULTIBOOT2_ARCH_I386    equ 0
MULTIBOOT2_HEADER_LEN   equ (multiboot2_header_end - multiboot2_header_start)
MULTIBOOT2_CHECKSUM     equ -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH_I386 + MULTIBOOT2_HEADER_LEN)

; ─── Stack Size ──────────────────────────────────────────────
STACK_SIZE              equ 0x4000          ; 16 KB initial stack

; ─── Sections ────────────────────────────────────────────────
section .multiboot
align 8
multiboot2_header_start:
    dd MULTIBOOT2_MAGIC
    dd MULTIBOOT2_ARCH_I386
    dd MULTIBOOT2_HEADER_LEN
    dd MULTIBOOT2_CHECKSUM

    ; ── Framebuffer tag ──────────────────────────────────────
    align 8
    dw 5                ; type  = framebuffer
    dw 0                ; flags = 0
    dd 20               ; size
    dd 1024             ; width  (preferred)
    dd 768              ; height (preferred)
    dd 32               ; depth  (bits per pixel)

    ; ── End tag ──────────────────────────────────────────────
    align 8
    dw 0                ; type  = end
    dw 0                ; flags = 0
    dd 8                ; size
multiboot2_header_end:

; ─── BSS: Stack ──────────────────────────────────────────────
section .bss
align 16
stack_bottom:
    resb STACK_SIZE
stack_top:

; ─── Read-only data ──────────────────────────────────────────
section .rodata
msg_no_multiboot db "FATAL: Not loaded by a Multiboot2 bootloader!", 0
msg_boot_ok      db "Multiboot2 OK. Entering kernel...", 0

; ─── Text ────────────────────────────────────────────────────
section .text
global _start
extern kernel_main          ; defined in kernel/kernel.c
extern gdt_install          ; defined in kernel/gdt.c  (linked in)

_start:
    ; ── 1. Validate Multiboot2 magic in EAX ─────────────────
    cmp eax, 0x36d76289     ; Multiboot2 bootloader magic reply
    jne .no_multiboot

    ; ── 2. Save Multiboot info pointer (EBX) before we clobber regs
    mov esi, ebx            ; ESI = multiboot_info *

    ; ── 3. Set up stack ──────────────────────────────────────
    mov esp, stack_top
    xor ebp, ebp            ; mark bottom of call-frame chain

    ; ── 4. Clear EFLAGS ──────────────────────────────────────
    push 0
    popf

    ; ── 5. Zero BSS (boot.asm owns this before C runs) ───────
    call _zero_bss

    ; ── 6. Install GDT ───────────────────────────────────────
    call gdt_install

    ; ── 7. Reload segment registers with new GDT selectors ──
    mov ax, 0x10            ; data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ; far jump to flush CS
    jmp 0x08:.flush_cs
.flush_cs:

    ; ── 8. Call kernel_main(multiboot_info *) ────────────────
    push esi                ; arg: multiboot info ptr
    call kernel_main
    add esp, 4

    ; ── 9. If kernel_main returns — halt ─────────────────────
.halt:
    cli
    hlt
    jmp .halt

; ─── Panic: not loaded by Multiboot2 ─────────────────────────
.no_multiboot:
    ; Print error via BIOS-style VGA direct write (we have no kernel yet)
    mov edi, 0xB8000
    mov esi, msg_no_multiboot
    mov ah, 0x4F            ; white on red
.print_loop:
    lodsb
    test al, al
    jz .freeze
    mov [edi], ax
    add edi, 2
    jmp .print_loop
.freeze:
    cli
    hlt
    jmp .freeze

; ─── _zero_bss ───────────────────────────────────────────────
; Clears the BSS section to zero before any C code runs.
; Linker script exports __bss_start and __bss_end.
global _zero_bss
extern __bss_start
extern __bss_end
_zero_bss:
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    ret

; ─── CPU feature helpers (called from C via kernel.h) ────────
global cpu_halt
cpu_halt:
    cli
    hlt
    jmp cpu_halt

global cpu_enable_interrupts
cpu_enable_interrupts:
    sti
    ret

global cpu_disable_interrupts
cpu_disable_interrupts:
    cli
    ret

global cpu_read_eflags
cpu_read_eflags:
    pushfd
    pop eax
    ret

global cpu_write_cr3
cpu_write_cr3:
    mov eax, [esp+4]
    mov cr3, eax
    ret

global cpu_read_cr2
cpu_read_cr2:
    mov eax, cr2
    ret

global cpu_read_cr3
cpu_read_cr3:
    mov eax, cr3
    ret

global cpu_enable_paging
cpu_enable_paging:
    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax
    ret

global cpu_disable_paging
cpu_disable_paging:
    mov eax, cr0
    and eax, 0x7FFFFFFF
    mov cr0, eax
    ret

; ─── I/O port helpers ────────────────────────────────────────
global outb
outb:                       ; void outb(uint16_t port, uint8_t val)
    mov dx, [esp+4]
    mov al, [esp+8]
    out dx, al
    ret

global inb
inb:                        ; uint8_t inb(uint16_t port)
    mov dx, [esp+4]
    xor eax, eax
    in  al, dx
    ret

global outw
outw:                       ; void outw(uint16_t port, uint16_t val)
    mov dx, [esp+4]
    mov ax, [esp+8]
    out dx, ax
    ret

global inw
inw:                        ; uint16_t inw(uint16_t port)
    mov dx, [esp+4]
    xor eax, eax
    in  ax, dx
    ret

global outl
outl:                       ; void outl(uint16_t port, uint32_t val)
    mov dx, [esp+4]
    mov eax, [esp+8]
    out dx, eax
    ret

global inl
inl:                        ; uint32_t inl(uint16_t port)
    mov dx, [esp+4]
    xor eax, eax
    in  eax, dx
    ret

global io_wait
io_wait:                    ; small delay via unused port 0x80
    xor al, al
    out 0x80, al
    ret

; ─── ISR stubs (for IDT — 32 exceptions + 16 IRQs) ──────────
; Each stub pushes an error code (or dummy) + vector number,
; then calls the common C handler: isr_handler(registers_t *)

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push dword 0            ; dummy error code
    push dword %1           ; interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
                            ; CPU already pushed error code
    push dword %1
    jmp isr_common_stub
%endmacro

%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword 0
    push dword %2
    jmp isr_common_stub
%endmacro

ISR_NOERRCODE  0   ; Divide by zero
ISR_NOERRCODE  1   ; Debug
ISR_NOERRCODE  2   ; NMI
ISR_NOERRCODE  3   ; Breakpoint
ISR_NOERRCODE  4   ; Overflow
ISR_NOERRCODE  5   ; Bound range exceeded
ISR_NOERRCODE  6   ; Invalid opcode
ISR_NOERRCODE  7   ; Device not available
ISR_ERRCODE    8   ; Double fault
ISR_NOERRCODE  9   ; Coprocessor segment overrun
ISR_ERRCODE   10   ; Invalid TSS
ISR_ERRCODE   11   ; Segment not present
ISR_ERRCODE   12   ; Stack-segment fault
ISR_ERRCODE   13   ; General protection fault
ISR_ERRCODE   14   ; Page fault
ISR_NOERRCODE 15   ; Reserved
ISR_NOERRCODE 16   ; x87 FPU error
ISR_ERRCODE   17   ; Alignment check
ISR_NOERRCODE 18   ; Machine check
ISR_NOERRCODE 19   ; SIMD FP exception
ISR_NOERRCODE 20   ; Virtualization exception
ISR_NOERRCODE 21   ; Control protection exception
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30   ; Security exception
ISR_NOERRCODE 31

IRQ  0, 32   ; PIT Timer
IRQ  1, 33   ; Keyboard
IRQ  2, 34   ; Cascade
IRQ  3, 35   ; COM2
IRQ  4, 36   ; COM1
IRQ  5, 37   ; LPT2
IRQ  6, 38   ; Floppy
IRQ  7, 39   ; LPT1 / Spurious
IRQ  8, 40   ; CMOS RTC
IRQ  9, 41   ; Free
IRQ 10, 42   ; Free
IRQ 11, 43   ; Free
IRQ 12, 44   ; PS/2 Mouse
IRQ 13, 45   ; FPU
IRQ 14, 46   ; ATA Primary
IRQ 15, 47   ; ATA Secondary

extern isr_handler          ; void isr_handler(registers_t *)
isr_common_stub:
    pusha                   ; push EAX ECX EDX EBX ESP EBP ESI EDI
    mov ax, ds
    push eax                ; save data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp                ; arg: pointer to registers_t on stack
    call isr_handler
    add esp, 4
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8              ; pop error code + interrupt number
    iret
