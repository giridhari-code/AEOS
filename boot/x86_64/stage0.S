/*
 * AEOS x86_64 - Stage 0 (32-bit multiboot loader)
 *
 * qemu -kernel / GRUB "multiboot" only accept 32-bit ELF, so this
 * tiny stub is what the bootloader sees. It carries the real
 * 64-bit kernel as an embedded blob (section .kpayload), copies it
 * to 1MB and far-jumps into its 32-bit entry (_start in boot.asm),
 * which performs the long-mode switch itself.
 *
 * Multiboot gives us flat 32-bit protected mode with interrupts off.
 */

.section .multiboot,"a",@progbits
.align 4
mb_header:
    .long 0x1BADB002            /* multiboot v1 magic */
    .long 0x00000003            /* flags: page-align + meminfo */
    .long -(0x1BADB002 + 3)     /* checksum */

.section .text
.code32
.global _start
_start:
    cli
    mov  $stack_top, %esp

    /* Copy payload -> 1MB. Source/size symbols come from the linker. */
    mov  $kpayload_start, %esi
    mov  $0x100000, %edi
    mov  $kpayload_size, %ecx
    mov  (%ecx), %ecx           /* size is a linker-set value */
    shr  $2, %ecx               /* word count */
1:
    mov  (%esi), %eax
    mov  %eax, (%edi)
    add  $4, %esi
    add  $4, %edi
    sub  $1, %ecx
    jnz  1b

    /* Far jump into the copied kernel's 32-bit entry at 1MB */
    ljmp $0x08, $0x100000

2:  hlt
    jmp  2b

.section .bss
.align 16
stack_bottom:
    .skip 8192
stack_top:

.section .payload,"a",@progbits
.global kpayload_start
kpayload_start:
    .incbin "build/aeos-x86.bin"
.global kpayload_end
kpayload_end:

/* Linker-provided: payload byte count */
.section .ksize,"a",@progbits
.align 4
.global kpayload_size
kpayload_size:
    .long kpayload_end - kpayload_start
