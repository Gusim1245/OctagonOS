; boot.asm
bits 32

; ─── Multiboot Header ─────────────────────────────────────────────────────────
section .multiboot
align 4
    dd 0x1BADB002       ; MAGIC
    dd 0x0              ; FLAGS
    dd -(0x1BADB002)    ; CHECKSUM

; ─── Text ─────────────────────────────────────────────────────────────────────
section .text

global _start
extern kernel_main
extern pit_handler_c
extern keyboard_handler_c

global keyboard_isr
keyboard_isr:
    pusha
    call keyboard_handler_c
    popa
    iret

global pit_isr
pit_isr:
    pusha
    call pit_handler_c
    popa
    iret

_start:
    cli
    mov al, 0xFF
    out 0x21, al
    out 0xA1, al
    mov esp, stack_top
    call kernel_main
    cli
    hlt

; ─── BSS ──────────────────────────────────────────────────────────────────────
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
