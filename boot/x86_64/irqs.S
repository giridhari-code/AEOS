/*
 * AEOS x86_64 - IDT stubs + interrupt gates
 *
 * Vectors 0-31: CPU exceptions. Vector 32: PIT timer (IRQ0 after
 * PIC remap). Each stub pushes a dummy error code where the CPU
 * does not, saves the volatile GPRs, calls irq_handler_c(vector,
 * frame), restores and iretq.
 */

.section .text

.extern g_prend
.extern preempt_thunk

/* ---- common save/restore (volatile regs only; C is ours) ---- */
.macro SAVE_FRAME
    push %rax
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
.endm

.macro LOAD_FRAME
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rax
.endm

.macro VECTOR n has_err=0
vector_\n:
.if \has_err == 0
    push $0                 /* dummy error code */
.endif
    push $\n                /* vector number */
    SAVE_FRAME
    mov  %rsp, %rdi         /* frame */
    cld
    call irq_handler_c
    LOAD_FRAME
    add  $16, %rsp          /* vector + error code */
    /* Preemption hook: if the tick handler armed g_prend, the
     * interrupted task's RIP is redirected through the yield
     * thunk (full state preserved; see switch.S). */
    mov  g_prend(%rip), %rax
    test %rax, %rax
    jz   7f
    mov  %rax, (%rsp)       /* frame RIP -> preempt_thunk */
    xor  %rax, %rax
    mov  %rax, g_prend(%rip)
7:  iretq
.endm

.global vector_table
.align 8
/* Pointer table: C side reads vector_table[i] as handler ADDRESSES.
 * NOTE: only slots 0..32 are populated; higher vectors must be
 * registered via direct symbols (see vector_128). */
vector_table:
    .quad vector_0
    .quad vector_1
    .quad vector_2
    .quad vector_3
    .quad vector_4
    .quad vector_5
    .quad vector_6
    .quad vector_7
    .quad vector_8
    .quad vector_9
    .quad vector_10
    .quad vector_11
    .quad vector_12
    .quad vector_13
    .quad vector_14
    .quad vector_15
    .quad vector_16
    .quad vector_17
    .quad vector_18
    .quad vector_19
    .quad vector_20
    .quad vector_21
    .quad vector_22
    .quad vector_23
    .quad vector_24
    .quad vector_25
    .quad vector_26
    .quad vector_27
    .quad vector_28
    .quad vector_29
    .quad vector_30
    .quad vector_31
    .quad vector_32

/* ---- stub bodies (referenced by vector_table above) ---- */
VECTOR 0
VECTOR 1
VECTOR 2
VECTOR 3
VECTOR 4
VECTOR 5
VECTOR 6
VECTOR 7
VECTOR 8  1             /* double fault w/ error code */
VECTOR 9
VECTOR 10 1             /* invalid TSS */
VECTOR 11 1             /* segment not present */
VECTOR 12 1             /* stack fault */
VECTOR 13 1             /* general protection fault */
VECTOR 14 1             /* page fault */
VECTOR 15
VECTOR 16
VECTOR 17 1
VECTOR 18
VECTOR 19
VECTOR 20
VECTOR 21
VECTOR 22
VECTOR 23
VECTOR 24
VECTOR 25
VECTOR 26
VECTOR 27
VECTOR 28
VECTOR 29
VECTOR 30
VECTOR 31
VECTOR 32               /* PIT timer */
/* Syscall gate stub: routes to int80_handler_c, not irq_handler_c */
.global vector_128
vector_128:
    push $0                  /* dummy error code */
    push $128
    SAVE_FRAME
    mov  %rsp, %rdi
    cld
    call int80_handler_c
    LOAD_FRAME
    add  $16, %rsp
    iretq

.global irq_enable
irq_enable:
    sti
    ret

.global irq_disable
irq_disable:
    cli
    ret
