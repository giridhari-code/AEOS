/*
 * AEOS - Process Model (Phase 1)
 *
 * A process wraps a scheduler task with:
 *   - its own address space (user window at USER_VA_BASE),
 *   - a unique pid and parent link,
 *   - an exit status kept until the parent reaps it (zombie).
 *
 * Creation is exec-style: proc_spawn_image() copies a flat program
 * image into fresh physical pages mapped RX at USER_VA_BASE plus a
 * private RW stack below USER_VA_STACK. fork()-style duplication
 * needs per-task kernel-stack tracking and arrives later.
 */

#ifndef AEOS_PROC_H
#define AEOS_PROC_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PROCS     8
#define PROC_NAME_LEN 12
#define USER_STACK_PAGES 2
#define PROC_FD_MAX   8

/*
 * Load `image` (size bytes of position-independent-at-USER_VA_BASE
 * machine code) as a new process named `name`. entry_off is the
 * byte offset of the entry point inside the image. Returns pid or
 * -1 on failure.
 */
int  proc_spawn_image(const char *name, const void *image,
                      size_t size, size_t entry_off);

/* Terminate the CALLING task's process (EL0 SYS_EXIT path). */
void proc_exit(int code) __attribute__((noreturn));

/* Block the calling task until `pid` is zombie; reap it and store
 * the exit code in *code_out when non-NULL. Returns the reaped pid
 * or -1 (no such child / interrupted by table reset). */
int  proc_wait(int pid, int *code_out);

/* Pid of the calling task; kernel threads report 0. */
int  proc_getpid(void);

/* Process table dump for the shell / reports. */
void proc_report(void);

/* Per-process file descriptor operations.
 * These operate on the CALLING task's process. Kernel threads (pid 0)
 * share a small global fd table for backward compatibility. */
struct device;  /* forward decl to avoid pulling in device.h */
int  proc_fd_alloc(struct device *dev);
struct device *proc_fd_get(int fd);
void proc_fd_close(int fd);

#endif /* AEOS_PROC_H */
