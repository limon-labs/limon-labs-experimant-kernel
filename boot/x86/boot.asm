; ============================================================
; AHK OS — Self-Healing Kernel
; File: boot.asm
; Description: Kernel Entry Point (after bootloader)
; ============================================================

bits 32
section .text
global _start
extern ahk_kernel_main

; ═══════════════════════════════════════════════════════════════
;  Kernel Entry Point
; ═══════════════════════════════════════════════════════════════
_start:
    ; Stack setup (0x90000 — kernel stack)
    mov esp, 0x90000
    xor ebp, ebp

    ; Save boot info pointer (from kernel_loader)
    mov esi, eax          ; eax = boot_info structure address

    ; Clear BSS (stack is already set — safe)
    call _zero_bss

    ; Call kernel main
    push esi              ; push boot_info pointer
    call ahk_kernel_main
    add esp, 4

.halt:
    cli
    hlt
    jmp .halt

; ═══════════════════════════════════════════════════════════════
;  BSS Zeroing
; ═══════════════════════════════════════════════════════════════
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

; ═══════════════════════════════════════════════════════════════
;  GDT and Segment Reload
;  Note: Now done in gdt.c (gdt_install)
; ═══════════════════════════════════════════════════════════════
