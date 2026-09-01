/*
 * AEOS - SMP secondary core entry
 *
 * PSCI CPU_ON lands here with x0 = context id (our AP slot index,
 * 1-based). Interrupts stay masked on APs in this phase: they run
 * a polling job loop, no timer ticks, no preemption.
 */

.section .text
.global smp_entry
smp_entry:
    msr     daifset, #0xF           /* mask DAIF completely       */

    /* Per-CPU kernel stack: ap_stacks[ctx-1], 4KB each. */
    sub     x6, x0, #1
    lsl     x6, x6, #12             /* *4096                      */
    adrp    x7, ap_stacks
    add     x7, x7, :lo12:ap_stacks
    add     sp, x7, x6

    /* Vector table must be per-core. */
    adrp    x7, exception_vector
    add     x7, x7, :lo12:exception_vector
    msr     vbar_el1, x7
    isb

    mov     x19, x0                 /* keep context id            */
    bl      smp_main                /* x0 = context id            */
1:  wfe
    b       1b
