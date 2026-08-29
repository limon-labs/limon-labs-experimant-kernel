; ============================================================
; AHK OS — Self-Healing Kernel
; File: boot/x86/boot_sector.asm
; Description: 512-byte boot sector
; ============================================================

[org 0x7C00]
bits 16

; ── Constants ─────────────────────────────────────────────────
BOOT_DRIVE equ 0x7C3C    ; Fixed boot drive storage location

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; ── Save boot drive at fixed location ────────────────────
    mov [BOOT_DRIVE], dl

    ; ── Load second stage (sector 2-11, 10 sectors) to 0x1000 ──
    mov ah, 0x02
    mov al, 10
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [BOOT_DRIVE]
    mov bx, 0x1000
    int 0x13
    jc disk_error

    ; ── Far jump to second stage ─────────────────────────────
    jmp 0x0000:0x1000

disk_error:
    mov si, msg_error
    call print
    jmp $

print:
    lodsb
    or al, al
    jz done
    mov ah, 0x0E
    int 0x10
    jmp print
done:
    ret

msg_error: db "Disk Error!", 0

times 510-($-$$) db 0
dw 0xAA55
