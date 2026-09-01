/*
 * AEOS - Kernel Shell (public API)
 *
 * An interactive console running as a kernel task. Reads a line
 * from the UART (yielding while idle, so it plays nicely with the
 * cooperative scheduler), parses it, and dispatches commands
 * against the kernel subsystems.
 */

#ifndef AEOS_SHELL_H
#define AEOS_SHELL_H

/* Spawn the shell as a task on the given stack.
 * Returns task index or -1. */
int shell_start(void *stack, unsigned stack_len);

#endif /* AEOS_SHELL_H */
