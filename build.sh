#!/bin/bash
set -e
rm -f *.o kernel.bin
nasm -f elf32 boot.asm -o boot.o
gcc -m32 -ffreestanding -fno-stack-protector -nostdlib -nostdinc \
    -fno-builtin -fno-pic -fno-pie \
    -Wall -Wextra -I. \
    -c kernel.c -o kernel.o
ld -m elf_i386 -T linker.ld --nmagic -o kernel.bin boot.o kernel.o
cp kernel.bin iso/boot/
grub-mkrescue -o Octagon_x86_64_beta.iso iso
qemu-system-i386 -kernel kernel.bin -no-reboot
