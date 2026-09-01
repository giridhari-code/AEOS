/*
 * AEOS-BIOS - Entry
 * Architecture: ARM64 (AArch64)
 *
 * Runs from flash at 0x00000000. Brings the CPU to EL1 with a RAM
 * stack, copies .data out of flash, clears .bss, then calls
 * bios_main(). Never returns.
 */

.section ".text.boot"

.global _start
_start:
    /* Park secondary cores immediately (same protocol as the
     * kernel's boot.S: MPIDR affinity == 0 is the boot core). */
    mrs     x5, mpidr_el1
    ubfx    x6, x5, #0, #24     /* Aff2.Aff1.Aff0 */
    ubfx    x7, x5, #32, #8     /* Aff3 */
    orr     x5, x6, x7, lsl #24
    cbz     x5, primary

secondary_park:
    wfe
    b       secondary_park

primary:
    /* Which EL? Drop EL2 -> EL1 if needed (kernel does its own
     * check too; doing it here keeps bios_main simple). */
    mrs     x4, CurrentEL
    cmp     x4, #0xc
    b.eq    park
    cmp     x4, #0x8
    b.ne    at_el1

    /* Allow EL1 physical counter access, no traps/routing. */
    mrs     x2, cnthctl_el2
    orr     x2, x2, #(1 << 1)
    orr     x2, x2, #(1 << 0)
    msr     cnthctl_el2, x2
    msr     hcr_el2, xzr
    msr     sctlr_el1, xzr
    mov     x2, #0x3c5          /* EL1h, DAIF masked */
    msr     spsr_el2, x2
    adr     x3, at_el1
    msr     elr_el2, x3
    eret

park:
    wfe
    b       park

at_el1:
    msr     spsel, #1

    adrp    x1, __stack_top
    add     x1, x1, :lo12:__stack_top
    mov     sp, x1

    /* Copy .data from its flash load image into RAM. */
    adrp    x2, __data_load
    add     x2, x2, :lo12:__data_load
    adrp    x3, __data_start
    add     x3, x3, :lo12:__data_start
    adrp    x4, __data_end
    add     x4, x4, :lo12:__data_end
1:  cmp     x3, x4
    b.hs    2f
    ldrb    w5, [x2], #1
    strb    w5, [x3], #1
    b       1b

    /* Clear .bss. */
2:  adrp    x2, __bss_start
    add     x2, x2, :lo12:__bss_start
    adrp    x3, __bss_end
    add     x3, x3, :lo12:__bss_end
3:  cmp     x2, x3
    b.hs    4f
    strb    wzr, [x2], #1
    b       3b

4:  bl      bios_main
5:  wfe
    b       5b
