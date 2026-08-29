; ============================================================
; AHK OS — Self-Healing Kernel
; File: boot/x86/second_stage.asm
; Description: 16-bit → 32-bit protected mode + kernel load
; ============================================================

[org 0x1000]
bits 16

; ── Constants ─────────────────────────────────────────────────
BOOT_DRIVE equ 0x7C3C    ; Same fixed location as boot sector
KERNEL_ENTRY equ 0x100000

start:
    cli
    ; ── Read boot drive from fixed location ──────────────────
    mov dl, [BOOT_DRIVE]

    ; ── Enable A20 line ─────────────────────────────────────
    in al, 0x92
    or al, 0x02
    out 0x92, al

    ; ── Load GDT ────────────────────────────────────────────
    lgdt [gdt_ptr]

    ; ── Switch to protected mode ─────────────────────────────
    mov eax, cr0
    or eax, 0x01
    mov cr0, eax

    ; ── Far jump to 32-bit code ─────────────────────────────
    jmp 0x08:protected_mode

bits 32
protected_mode:
    ; ── Reload segment registers ─────────────────────────────
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; ── Set up boot info structure (at 0x3000) ──────────────
    mov eax, 0x41484B31      ; magic 0xAHK1
    mov [0x3000], eax

    ; Save kernel size
    mov eax, 50 * 512
    mov [0x3004], eax

    ; Entry point
    mov eax, KERNEL_ENTRY
    mov [0x3008], eax

    ; Stack top
    mov eax, 0x90000
    mov [0x300C], eax

    ; Memory map (placeholder)
    mov eax, 0x1000
    mov [0x3010], eax

    ; Framebuffer
    mov eax, 0xB8000
    mov [0x3014], eax

    ; ── Jump to kernel ───────────────────────────────────────
    jmp KERNEL_ENTRY

; ── GDT ───────────────────────────────────────────────────────
gdt_start:
    dq 0                    ; null descriptor
    dw 0xFFFF, 0x0000, 0x9A, 0xCF  ; code descriptor
    dw 0xFFFF, 0x0000, 0x92, 0xCF  ; data descriptor
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_start - 1
    dd gdt_start
