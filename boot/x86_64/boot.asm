/*
 * AEOS x86_64 - Multiboot Entry
 *
 * GRUB / qemu -kernel drops us in 32-bit protected mode with the
 * multiboot magic in eax and the info pointer in ebx. This stub
 * builds identity page tables (1GB pages), switches to 64-bit long
 * mode, then calls the kernel.
 */

.section .multiboot
.align 4
mb_header:
    .long 0x1BADB002            /* multiboot v1 magic */
    .long 0x00000000            /* flags: none */
    .long -(0x1BADB002)         /* checksum */

.section .bss
.align 4096
pml4:   .skip 4096
pdpt:   .skip 4096
boot_stack_bottom:
    .skip 16384
boot_stack_top:

.section .rodata
.align 16
.global gdt64
gdt64:
    .quad 0                     /* 0x00 null */
    .quad 0x00209A0000000000    /* 0x08 kernel code64 */
    .quad 0x0000920000000000    /* 0x10 kernel data */
    .quad 0x0020F80000000000    /* 0x18 user code64, DPL=3 */
    .quad 0x0000F20000000000    /* 0x20 user data,   DPL=3 */
    /* 0x28: 16-byte TSS descriptor, base patched by kernel C code */
    .quad 0x0000890000000000
    .quad 0
gdt64_end:
.global gdt64_ptr
gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .quad gdt64

.section .text
.code32
.global _start
_start:
    cli
    mov  $boot_stack_top, %esp

    /* Zero the kernel's .bss (stage0 loads PROGBITS only).
     * Page tables below live in .bss, so this must run first.
     * The loop itself uses no memory above %esp. */
    mov  $_bss_start, %edi
    mov  $_bss_end, %ecx
1:  cmp  %ecx, %edi
    jae  2f
    movb $0, (%edi)
    inc  %edi
    jmp  1b
2:
    /* CPUID: is long mode supported? */
    mov  $0x80000000, %eax
    cpuid
    cmp  $0x80000001, %eax
    jb   no_longmode
    mov  $0x80000001, %eax
    cpuid
    test $(1 << 29), %edx
    jz   no_longmode

    /* Page tables: identity-map the first 1GB with one huge page.
     *
     * SECURITY: Phase 0 uses a flat single-address-space model.
     * The U/S bit on the 1GB page grants ring-3 access to all memory.
     * This MUST be fixed in Phase 1 by creating separate user page
     * tables with restricted mappings (code RX, data RW, stack RW).
     * For now, at minimum, the PML4 entry is supervisor-only to add
     * a defense layer — ring-3 must go through the PDPT entry. */
    mov  $pml4, %edi
    mov  $pdpt, %eax
    or   $0x03, %eax            /* present | writable (supervisor only) -> PML4[0] */
    mov  %eax, (%edi)

    mov  $pdpt, %edi
    mov  $0x87, %eax            /* present | writable | user | 1GB page */
    mov  %eax, (%edi)           /* PDPT[0] = 0 .. 1GB */

    /* Enable PAE */
    mov  %cr4, %eax
    or   $(1 << 5), %eax
    mov  %eax, %cr4

    /* Point CR3 at PML4 */
    mov  $pml4, %eax
    mov  %eax, %cr3

    /* EFER.LME */
    mov  $0xC0000080, %ecx
    rdmsr
    or   $(1 << 8), %eax
    wrmsr

    /* Paging on (also keeps protection on) */
    mov  %cr0, %eax
    or   $(1 << 31), %eax
    mov  %eax, %cr0

    /* Load 64-bit GDT and far-jump into long mode */
    lgdt gdt64_ptr
    ljmp $0x08, $entry64

no_longmode:
    hlt
    jmp  no_longmode

.code64
entry64:
    xor  %ebp, %ebp
    mov  $boot_stack_top, %rsp

    /* Pass multiboot args: rdi = magic, rsi = mbi pointer.
     * eax/ebx survive the mode switch zero-extended. */
    mov  %eax, %edi
    mov  %ebx, %esi

    xor  %rbp, %rbp
    call x86_kernel_main

halt:
    cli
    hlt
    jmp  halt
