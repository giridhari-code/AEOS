/*
 * AEOS x86_64 - El Torito CD boot image ("real ISO" path)
 *
 * The ISO declares this image (code + embedded aeos-x86.bin
 * payload) as the no-emulation boot record, so the BIOS itself
 * loads EVERYTHING at 0x7C00 - no INT 13h involved anywhere.
 *
 * All this loader does:
 *   1. real-mode housekeeping (segments, stack),
 *   2. GDT + protected-mode switch,
 *   3. copy the payload half to 1MB,
 *   4. jump into the kernel's 32-bit entry (_start in boot.asm)
 *      which performs the long-mode switch - exactly like the
 *      multiboot stage0 path does.
 */

.set RELOC_BASE,    0               /* nothing to relocate        */
.set KERN_BASE,     0x100000        /* final kernel destination  */
.set PAYLOAD_SIZE,  kpayload_end - kpayload_start

.section .text
.code16
.globl _start
_start:
    cli
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0x6FF0, %sp                /* grows down, clear of code */
    sti
    cld

    lgdt gdt_desc

    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0

    ljmp $0x08, $pm32_cont

/* ---------------------------------------------------------------
 * 32-bit continuation.
 * --------------------------------------------------------------- */
.code32
pm32_cont:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    mov $0x90000, %esp

    /* Payload: 0x7C00+(kpayload_start-_start) -> 1MB. */
    mov $(0x7C00 + kpayload_start - _start), %esi
    mov $KERN_BASE, %edi
    mov $PAYLOAD_SIZE, %ecx
    shr $2, %ecx
    rep movsl

    mov $KERN_BASE, %eax
    jmp *%eax                       /* kernel _start (32-bit)    */

hang:
    hlt
    jmp hang

/* ---------------------------------------------------------------
 * Data
 * --------------------------------------------------------------- */
.align 8
gdt:
    .quad 0                         /* null                      */
    .quad 0x00CF9A000000FFFF        /* code: base 0, limit 4GB   */
    .quad 0x00CF92000000FFFF        /* data                      */
gdt_end:

gdt_desc:
    .word gdt_end - gdt - 1
    .long gdt
    .word 0

.section .payload,"a"
.global kpayload_start
kpayload_start:
    .incbin "boot/x86_64/build/aeos-x86.bin"
.global kpayload_end
kpayload_end:
.global payload_end
payload_end:
