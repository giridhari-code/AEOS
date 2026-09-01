/*
 * AEOS x86_64 - MBR boot code for the GPT disk image
 *
 * Lives in LBA 0 of build/aeos.img inside the protective MBR's
 * boot-code area (446 bytes). The BIOS hands control here in real
 * mode with DL = boot drive. We extended-read the kernel payload
 * (aeos-x86.bin, placed by make_img.py at KERNEL_LBA) into low
 * memory, switch on protected mode, copy it to 1MB and jump into
 * its 32-bit entry - the same contract the multiboot stage0 uses.
 *
 * Budget: everything below must fit in 440 bytes.
 */

.set RELOC_BASE,    0x20000         /* staging area in low RAM   */
.set KERN_BASE,     0x100000        /* final kernel destination */
.set CHUNK_SECTORS, 32              /* 16KB per INT 13h call     */

/* Provided via as --defsym by tools/build/make_img.py:
 *   KERNEL_LBA, KERNEL_SECTORS                                   */

.section .text
.code16
.globl _start
_start:
    mov %dl, boot_drive             /* BIOS gives the disk in DL */

    cli
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0x6FF0, %sp
    sti
    cld

    xor %bp, %bp                    /* sectors copied            */

read_loop:
    mov $KERNEL_SECTORS, %cx
    sub %bp, %cx
    jz read_done
    cmp $CHUNK_SECTORS, %cx
    jbe do_read
    mov $CHUNK_SECTORS, %cx

do_read:
    lea dap, %si
    mov %cx, 2(%si)                 /* +2 count                  */
    movw $0, 4(%si)                 /* +4 buffer offset          */
    mov %bp, %ax
    shl $5, %ax                     /* paragraphs done           */
    add $(RELOC_BASE >> 4), %ax
    mov %ax, 6(%si)                 /* +6 buffer segment         */
    mov $KERNEL_LBA, %ax
    add %bp, %ax
    mov %ax, 8(%si)                 /* +8 LBA                    */
    movw $0, 12(%si)

    mov boot_drive, %dl
    mov $0x42, %ah
    int $0x13
    jc fail

    add %cx, %bp
    jmp read_loop

read_done:
    lgdt gdt_desc

    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0

    ljmp $0x08, $pm32

.code32
pm32:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov $0x90000, %esp

    /* Copy staging -> final kernel home at 1MB. */
    mov $RELOC_BASE, %esi
    mov $KERN_BASE, %edi
    mov $KERNEL_SECTORS, %ecx
    shl $7, %ecx                    /* sectors * 512 / 4 dwords  */
    rep movsl

    mov $KERN_BASE, %eax
    jmp *%eax                       /* kernel _start (32-bit)    */

fail:
    hlt
    jmp fail

.align 8
gdt:
    .quad 0
    .quad 0x00CF9A000000FFFF
    .quad 0x00CF92000000FFFF
gdt_end:

gdt_desc:
    .word gdt_end - gdt - 1
    .long gdt
    .word 0

.align 8
dap:
    .byte 0x10, 0
    .word 0                         /* count                     */
    .word 0                         /* offset                    */
    .word 0                         /* segment                   */
    .long 0, 0                      /* LBA                       */

boot_drive:
    .byte 0x80
