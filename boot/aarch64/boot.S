/*
 * AEOS - Boot Assembly
 * Architecture: ARM64 (AArch64)
 * Target: QEMU virt machine
 *
 * Entry point. Parks secondary cores, detects current exception
 * level, drops EL2->EL1 if needed, sets up stack, clears BSS,
 * calls loader_main then kernel_main.
 *
 * QEMU's -kernel stub enters at EL2 when the CPU implements it
 * (cortex-a72 does). All of AEOS runs at EL1, so we must ERET
 * down before touching EL1-only state such as VBAR_EL1.
 */

.section ".text.boot"

.global _start
_start:
    /*
     * Park secondary cores immediately: every CPU enters here with
     * the same PC. MPIDR affinity fields == 0 identifies the boot
     * core (bits 31/30 hold RES1/U flags, not affinity). Parked
     * secondaries sit in wfe; SMP bring-up will release them via SEV
     * from the kernel once per-CPU state exists.
     */
    mrs     x5, mpidr_el1
    ubfx    x6, x5, #0, #24     /* Aff2.Aff1.Aff0 */
    ubfx    x7, x5, #32, #8     /* Aff3 */
    orr     x5, x6, x7, lsl #24
    cbz     x5, primary

secondary_park:
    wfe
    b       secondary_park

primary:
    /* Boot trace: 'B' */
    mov     x9, #0x09000000
    mov     w10, #'B'
    strb    w10, [x9]

    /* Which EL are we at? CurrentEL: EL1=0x4, EL2=0x8, EL3=0xC.
     * x4, not x0: firmware hands the DTB pointer in x0 (ARM boot
     * protocol) and it must survive untouched until loader_main. */
    mrs     x4, CurrentEL
    cmp     x4, #0xc
    b.eq    park                /* EL3 unsupported */
    cmp     x4, #0x8
    b.ne    at_el1              /* already EL1 (or lower) */

    /* Boot trace: '2' means entered at EL2 */
    mov     w10, #'2'
    strb    w10, [x9]

    /* ---- Drop EL2 -> EL1 ---- */

    /* Allow EL1 access to the physical counter/timer registers */
    mrs     x2, cnthctl_el2
    orr     x2, x2, #(1 << 1)   /* EL1PCEN: phys counter/timer reg access */
    orr     x2, x2, #(1 << 0)   /* EL1PCTEN: phys counter access */
    msr     cnthctl_el2, x2

    /* No stage-2 traps or interrupt routing: IRQ/FIQ land at EL1 */
    msr     hcr_el2, xzr

    /* Route EL2's own exceptions to the same vector table */
    adrp    x2, exception_vector
    add     x2, x2, :lo12:exception_vector
    msr     vbar_el2, x2

    /* Clean EL1 state: MMU/caches off, little-endian */
    msr     sctlr_el1, xzr

    /* SPSR_EL2: enter EL1h with DAIF masked */
    mov     x2, #0x3c5
    msr     spsr_el2, x2

    /* ELR_EL2 = continuation point below */
    adr     x3, at_el1
    msr     elr_el2, x3
    eret

park:
    wfe
    b       park

at_el1:
    /* Use SP_ELx at EL1 so exceptions vector through the
     * "Current EL with SP_ELx" entries (not the parked SP_EL0 ones). */
    msr     spsel, #1

    /* Stack pointer */
    adrp    x1, _stack_top
    add     x1, x1, :lo12:_stack_top
    mov     sp, x1

    /* Clear BSS */
    adrp    x2, _bss_start
    add     x2, x2, :lo12:_bss_start
    adrp    x3, _bss_end
    add     x3, x3, :lo12:_bss_end
1:  cmp     x2, x3
    b.ge    2f
    strb    wzr, [x2], #1
    b       1b

2:  /* Stage 2: AEOS-LDR. x0 = firmware DTB pointer (ARM boot
     * protocol); loader validates the image and returns a
     * bootinfo_t pointer in x0 for the kernel. */
    bl      loader_main
    cbz     x0, park            /* loader validation failed */

    bl      kernel_main

3:  wfe
    b       3b
