/*
 * AEOS - Exception Vector Table
 * Architecture: ARM64 (AArch64)
 *
 * VBAR_EL1 points here. Each entry is exactly 0x80 bytes.
 *
 * ARCHITECTURAL LAYOUT (do not "simplify"):
 *   0x000-0x1FF : Current EL using SP_EL0  (parked: we never run on SP_EL0)
 *   0x200-0x3FF : Current EL using SP_ELx  (our live handlers)
 *   0x400-0x5FF : Lower EL, AArch64        (parked)
 *   0x600-0x7FF : Lower EL, AArch32        (parked)
 *
 * Within each 0x200 group: +0x00 sync, +0x80 IRQ, +0x100 FIQ,
 * +0x180 SError.
 */

.section .text

/* ============================================================
 * Save/restore macros
 * ============================================================
 * Frame: x0-x18 plus x30 (LR), 20 slots, 160 bytes.
 * x30 MUST be saved: the stub's own `bl` would otherwise destroy
 * the interrupted context's return address.
 */

.macro SAVE_REGS
    sub     sp, sp, #20 * 8
    stp     x0, x1,   [sp, #0 * 8]
    stp     x2, x3,   [sp, #2 * 8]
    stp     x4, x5,   [sp, #4 * 8]
    stp     x6, x7,   [sp, #6 * 8]
    stp     x8, x9,   [sp, #8 * 8]
    stp     x10, x11, [sp, #10 * 8]
    stp     x12, x13, [sp, #12 * 8]
    stp     x14, x15, [sp, #14 * 8]
    stp     x16, x17, [sp, #16 * 8]
    str     x18,      [sp, #18 * 8]
    str     x30,      [sp, #19 * 8]
    mov     x0, sp
.endm

.macro RESTORE_REGS
    ldp     x0, x1,   [sp, #0 * 8]
    ldp     x2, x3,   [sp, #2 * 8]
    ldp     x4, x5,   [sp, #4 * 8]
    ldp     x6, x7,   [sp, #6 * 8]
    ldp     x8, x9,   [sp, #8 * 8]
    ldp     x10, x11, [sp, #10 * 8]
    ldp     x12, x13, [sp, #12 * 8]
    ldp     x14, x15, [sp, #14 * 8]
    ldp     x16, x17, [sp, #16 * 8]
    ldr     x18,      [sp, #18 * 8]
    ldr     x30,      [sp, #19 * 8]
    add     sp, sp, #20 * 8
.endm

/*
 * Lower-EL (exceptions FROM user mode) frames additionally carry
 * elr_el1/spsr_el1 on the stack. The banked registers alone are
 * NOT safe across a context switch: hardware rewrites them on any
 * nested exception entry (e.g. a timer tick landing between the
 * switch-in and the stub's final eret), which would destroy the
 * interrupted user PC. Storing them per-frame makes every task's
 * user-return state survive arbitrary switches.
 */
.macro SAVE_LOWER
    sub     sp, sp, #22 * 8
    stp     x0, x1,   [sp, #0 * 8]
    stp     x2, x3,   [sp, #2 * 8]
    stp     x4, x5,   [sp, #4 * 8]
    stp     x6, x7,   [sp, #6 * 8]
    stp     x8, x9,   [sp, #8 * 8]
    stp     x10, x11, [sp, #10 * 8]
    stp     x12, x13, [sp, #12 * 8]
    stp     x14, x15, [sp, #14 * 8]
    stp     x16, x17, [sp, #16 * 8]
    str     x18,      [sp, #18 * 8]
    str     x30,      [sp, #19 * 8]
    mrs     x2, elr_el1
    str     x2,       [sp, #20 * 8]
    mrs     x2, spsr_el1
    str     x2,       [sp, #21 * 8]
    mov     x0, sp
.endm

.macro RESTORE_LOWER
    ldr     x2,      [sp, #20 * 8]
    msr     elr_el1, x2
    ldr     x2,      [sp, #21 * 8]
    msr     spsr_el1, x2
    ldp     x0, x1,   [sp, #0 * 8]
    ldp     x2, x3,   [sp, #2 * 8]
    ldp     x4, x5,   [sp, #4 * 8]
    ldp     x6, x7,   [sp, #6 * 8]
    ldp     x8, x9,   [sp, #8 * 8]
    ldp     x10, x11, [sp, #10 * 8]
    ldp     x12, x13, [sp, #12 * 8]
    ldp     x14, x15, [sp, #14 * 8]
    ldp     x16, x17, [sp, #16 * 8]
    ldr     x18,      [sp, #18 * 8]
    ldr     x30,      [sp, #19 * 8]
    add     sp, sp, #22 * 8
.endm

/* ============================================================
 * Vector Table
 * ============================================================ */

.align 12  /* 4KB aligned for VBAR_EL1 */
.global exception_vector
exception_vector:

/* --- Current EL with SP_EL0 (unused: kernel runs on SP_ELx) --- */
.align 7
    b   handle_parked
.align 7
    b   handle_parked
.align 7
    b   handle_parked
.align 7
    b   handle_parked

/* --- Current EL with SP_ELx (live) --- */
.align 7  /* 0x200: Synchronous */
    b   handle_sync_entry

.align 7  /* 0x280: IRQ */
    b   handle_irq_entry

.align 7  /* 0x300: FIQ */
    b   handle_fiq_entry

.align 7  /* 0x380: SError */
    b   handle_serror_entry

/* --- Lower EL with AArch64 (exceptions FROM user mode, EL0) --- */
.align 7  /* 0x400: Synchronous (svc / faults from EL0) */
    b   handle_sync_lower

.align 7  /* 0x480: IRQ (timer ticks while in user mode) */
    b   handle_irq_lower

.align 7  /* 0x500: FIQ */
    b   handle_parked

.align 7  /* 0x580: SError */
    b   handle_serror_lower

/* --- Lower EL with AArch32 --- */
.align 7
    b   handle_parked
.align 7
    b   handle_parked
.align 7
    b   handle_parked
.align 7
    b   handle_parked


/* Parked-class entry: scream once over UART, then spin forever. */
handle_parked:
    adr     x0, parked_msg
1:  ldrb    w2, [x0], #1
    cbz     w2, 2f
    ldr     x3, =0x09000000     /* UART DR */
    strb    w2, [x3]
    b       1b
2:  wfe
    b       2b

parked_msg:
    .ascii "\n<<PARKED VECTOR HIT>>\n\0"


/* ============================================================
 * Exception Entry Stubs
 * ============================================================ */

handle_sync_entry:
    SAVE_REGS
    bl      sync_handler_c
    RESTORE_REGS
    eret

handle_irq_entry:
    SAVE_REGS
    bl      irq_handler_c
    RESTORE_REGS
    eret

handle_fiq_entry:
    SAVE_REGS
    bl      fiq_handler_c
    RESTORE_REGS
    eret

handle_serror_entry:
    SAVE_REGS
    bl      serror_handler_c
    RESTORE_REGS
    eret

/*
 * Lower-EL (user mode) entry points. On exception entry from EL0
 * the CPU switches to SP_EL1 automatically (SPSel=1), so these run
 * on the task's kernel stack. The C handlers may block (yield) -
 * the per-task context now carries ELR/SPSR across switches.
 */
handle_sync_lower:
    SAVE_LOWER
    bl      sync_lower_handler_c
    RESTORE_LOWER
    eret

handle_irq_lower:
    SAVE_LOWER
    bl      irq_handler_c
    RESTORE_LOWER
    eret

handle_serror_lower:
    SAVE_LOWER
    bl      serror_handler_c
    RESTORE_LOWER
    eret


/* ============================================================
 * Enable/disable interrupts (DAIF bits in PSTATE)
 * ============================================================ */

.global irq_enable
irq_enable:
    msr     daifclr, #2    /* Clear IRQ mask bit */
    ret

.global irq_disable
irq_disable:
    msr     daifset, #2    /* Set IRQ mask bit */
    ret
