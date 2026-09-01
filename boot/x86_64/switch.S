/*
 * AEOS x86_64 - Context Switch
 *
 * Same cooperative model as the ARM switch.S, SAME slot map as
 * sched.c crafts (shared code): regs[] indices -
 *   [0]  rbx  (holds task entry on FIRST entry - read by trampoline)
 *   [5]  rbp
 *   [11] resume/trampoline address (ARM uses this slot for LR)
 *   [12] rsp                        (ARM uses this slot for SP)
 * Remaining callee-saved regs ride in [1]-[4],[6].
 */

.section .text

/* void cpu_switch_to(task_struct_t *prev in rdi, next in rsi) */
.global cpu_switch_to
cpu_switch_to:
    /* save prev: rbx,r12,r13,r14,r15,rbp then sp@96(12), rip@88(11) */
    mov %rbx, 0(%rdi)
    mov %r12, 8(%rdi)
    mov %r13, 16(%rdi)
    mov %r14, 24(%rdi)
    mov %r15, 32(%rdi)
    mov %rbp, 40(%rdi)
    mov %rsp, 96(%rdi)
    lea .resume(%rip), %rax
    mov %rax, 88(%rdi)

    /* load next */
    mov 0(%rsi), %rbx
    mov 8(%rsi), %r12
    mov 16(%rsi), %r13
    mov 24(%rsi), %r14
    mov 32(%rsi), %r15
    mov 40(%rsi), %rbp
    mov 96(%rsi), %rsp
    mov 88(%rsi), %rax
    jmp *%rax

.resume:
    ret

/* void task_trampoline(void) - first entry only; rbx = entry fn */
.global task_trampoline
task_trampoline:
    call *%rbx
1:  jmp sched_exit
    jmp 1b

/* ============================================================
 * Preemption thunk: the timer stub rewrites the interrupted
 * task's saved RIP to this address when its quantum expires.
 * Sequence (single CPU, g_prend serializes):
 *   1. iretq restored ALL regs to pre-interrupt state
 *   2. we snapshot caller-saved regs into globals
 *   3. call sched_yield  -> switches away cooperatively;
 *      callee-saved regs travel in the task context
 *   4. on resume, restore caller-saved, ret -> original RIP
 */
.section .bss
.align 8
.global preempt_sv
preempt_sv:
    .skip 9 * 8          /* rax rcx rdx rsi rdi r8 r9 r10 r11 */

.section .text
.global preempt_thunk
.global g_in_thunk
g_in_thunk:
    .long 0

preempt_thunk:
    movl $1, g_in_thunk(%rip)
    mov %rax, preempt_sv+0(%rip)
    mov %rcx, preempt_sv+8(%rip)
    mov %rdx, preempt_sv+16(%rip)
    mov %rsi, preempt_sv+24(%rip)
    mov %rdi, preempt_sv+32(%rip)
    mov %r8,  preempt_sv+40(%rip)
    mov %r9,  preempt_sv+48(%rip)
    mov %r10, preempt_sv+56(%rip)
    mov %r11, preempt_sv+64(%rip)

    call sched_yield

    mov preempt_sv+0(%rip), %rax
    mov preempt_sv+8(%rip), %rcx
    mov preempt_sv+16(%rip), %rdx
    mov preempt_sv+24(%rip), %rsi
    mov preempt_sv+32(%rip), %rdi
    mov preempt_sv+40(%rip), %r8
    mov preempt_sv+48(%rip), %r9
    mov preempt_sv+56(%rip), %r10
    mov preempt_sv+64(%rip), %r11
    movl $0, g_in_thunk(%rip)
    ret                  /* back to the interrupted instruction */
