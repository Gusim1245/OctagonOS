; boot.asm
bits 32
section .text
align 4

MAGIC    equ 0x1BADB002
FLAGS    equ 0x0
CHECKSUM equ -(MAGIC + FLAGS)

dd MAGIC
dd FLAGS
dd CHECKSUM

global _start
extern kernel_main
extern pit_handler_c

global pit_isr
pit_isr:
    pusha
    call pit_handler_c
    popa
    iret

_start:
    cli                     ; desabilita interrupções

    ; mascara TODAS as IRQs antes de qualquer coisa
    mov al, 0xFF
    out 0x21, al
    out 0xA1, al

    mov esp, stack_top
    call kernel_main
    cli
    hlt

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
