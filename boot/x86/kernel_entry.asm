; ============================================================
; AHK OS — Self-Healing Kernel
; File: boot/x86/kernel_entry.asm
; Description: Kernel entry point
; ============================================================

bits 32
section .text
global _start
extern ahk_kernel_main

_start:
    mov esp, 0x90000
    xor ebp, ebp

    ; ── Save boot info pointer ───────────────────────────────
    mov esi, eax

    ; ── Clear BSS ────────────────────────────────────────────
    call _zero_bss

    ; ── Call kernel main ─────────────────────────────────────
    push esi
    call ahk_kernel_main
    add esp, 4

.halt:
    cli
    hlt
    jmp .halt

; ── BSS Zeroing ───────────────────────────────────────────────
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
