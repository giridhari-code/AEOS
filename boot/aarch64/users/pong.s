/*
 * AEOS user program: "pong"
 * Same runtime model as ping, different greeting and exit code 9.
 */

.global _start
.text

_start:
    mov     x20, #3               /* iterations */
    adr     x21, msg

loop:
    mov     x0, x21
    mov     x8, #1                /* SYS_PRINT */
    svc     #0

    mov     x22, #250             /* busy delay (nested) */
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

    mov     x0, #9                /* exit code */
    mov     x8, #4                /* SYS_EXIT */
    svc     #0
park:
    wfe
    b       park

msg:
    .asciz "[pong] process model alive\n"
