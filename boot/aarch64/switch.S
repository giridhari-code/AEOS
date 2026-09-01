/*
 * AEOS - Context Switch
 * Architecture: ARM64 (AArch64)
 *
 * cpu_switch_to(prev, next):
 *   Saves the callee-saved register file (x19-x28), frame pointer
 *   (x29), link register (x30) and SP into prev->cpu.regs[], then
 *   loads the same set from next->cpu.regs[] and returns - which
 *   enters the next task at whatever its saved LR is.
 *
 * Byte-exact slot map (task_struct_t.cpu.regs[], kernel/sched.h):
 *   [0]=x19 [1]=x20 [2]=x21 [3]=x22 [4]=x23 [5]=x24
 *   [6]=x25 [7]=x26 [8]=x27 [9]=x28 [10]=x29(fp) [11]=x30(lr)
 *   [12]=sp   ([13]/[14] reserved; ELR/SPSR live in exception
 *   frames on each task's kernel stack - see exceptions.S)
 *
 * task_trampoline:
 *   First-entry trampoline for spawned tasks. The scheduler places
 *   the task entry in slot 0 (restored into x19) and this
 *   trampoline in LR, so a returning entry function cannot fall
 *   into garbage: it exits through sched_exit() instead.
 */

.section .text

/*
 * void cpu_switch_to(task_struct_t *prev, task_struct_t *next)
 */
.global cpu_switch_to
cpu_switch_to:
    /* Save the live context into prev */
    stp     x19, x20, [x0, #0]
    stp     x21, x22, [x0, #16]
    stp     x23, x24, [x0, #32]
    stp     x25, x26, [x0, #48]
    stp     x27, x28, [x0, #64]
    stp     x29, x30, [x0, #80]
    mov     x2, sp
    str     x2,      [x0, #96]

    /* Load the next context from next */
    ldp     x19, x20, [x1, #0]
    ldp     x21, x22, [x1, #16]
    ldp     x23, x24, [x1, #32]
    ldp     x25, x26, [x1, #48]
    ldp     x27, x28, [x1, #64]
    ldp     x29, x30, [x1, #80]
    ldr     x2,      [x1, #96]
    mov     sp, x2

    ret

/*
 * void task_trampoline(void)
 *   x19 = task entry (from slot 0). Falls into sched_exit if the
 *   entry function ever returns.
 */
.global task_trampoline
task_trampoline:
    blr     x19
    b       sched_exit
