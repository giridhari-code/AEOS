/*
 * AEOS user program: "ping"
 * Runs at EL0 in its own address space. Prints a greeting, yields,
 * repeats, then exits with code 7. Syscall ABI (kernel/syscall.h):
 *   x8=1 SYS_PRINT (x0=msg)   x8=3 SYS_YIELD
 *   x8=4 SYS_EXIT  (x0=code)
 */

.global _start
.text

_start:
    mov     x20, #5               /* iterations */
    adr     x21, msg

loop:
    mov     x0, x21
    mov     x8, #1                /* SYS_PRINT */
    svc     #0

    mov     x22, #400             /* busy delay (nested) */
delay:
    mov     x23, #1000
inner:
    sub     x23, x23, #1
    cbnz    x23, inner
    sub     x22, x22, #1
    cbnz    x22, delay

    mov     x8, #3                /* SYS_YIELD */
    svc     #0

    sub     x20, x20, #1
    cbnz    x20, loop

    mov     x0, #7                /* exit code */
    mov     x8, #4                /* SYS_EXIT */
    svc     #0
park:
    wfe
    b       park

msg:
    .asciz "[ping] hello from EL0 process land\n"
