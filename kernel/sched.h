/*
 * AEOS - Scheduler (public API)
 *
 * Cooperative round-robin multitasking between kernel threads.
 *
 * Design contract (Phase 0):
 *   - Tasks are kernel threads: they share the single EL1 address
 *     space and run on their own private stacks.
 *   - Switching happens ONLY inside sched_yield(), called from task
 *     context at safe points. The timer tick accounts CPU time and
 *     flags an overdue task, but never switches stacks from inside
 *     the interrupt stub (that would corrupt the exception frame).
 *     Preemptive switching can be layered on later by moving the
 *     switch into the exception-return path.
 *   - Fixed task table, static stacks, no dynamic allocation.
 */

#ifndef AEOS_SCHED_H
#define AEOS_SCHED_H

#include <stdint.h>
#include <stddef.h>

#define MAX_TASKS        16
#define TASK_NAME_LEN    12

/* Preemption quantum: ticks a RUNNING task may hog before the
 * timer-driven preemer may force it off the CPU. ~25 = 250ms at
 * 100Hz. Watchdog (rogue kill) remains the last resort above. */
#define SCHED_QUANTUM    25

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_FINISHED,
} task_state_t;

/*
 * Saved CPU context. MUST stay at offset 0 of task_struct:
 * boot/aarch64/switch.S hardcodes these offsets.
 * Layout: x19-x28 (10), fp(x29), lr(x30), sp -> 13 x 8 bytes used;
 * slots 13/14 are reserved. ELR_EL1/SPSR_EL1 are NOT saved here:
 * they live in per-frame storage on each task's kernel stack
 * (exceptions.S SAVE_LOWER/RESTORE_LOWER) so nested interrupts can
 * never destroy a user task's return state.
 */
typedef struct {
    uint64_t regs[15];
} cpu_context_t;

typedef struct {
    cpu_context_t cpu;                  /* offset 0 - see switch.S */
    volatile task_state_t state;
    char name[TASK_NAME_LEN];
    uint32_t ticks_used;                /* CPU time accounting */
    uint32_t yields;
    uint32_t starved_ticks;             /* consecutive ticks w/o yield */
    uint8_t  kill_pending;              /* watchdog verdict */

    /* Process model (Phase 1). NULL as = kernel thread sharing the
     * flat identity address space. uentry/usp describe the EL0
     * drop-in point for user tasks and are consumed once by the
     * trampoline on first schedule. */
    void    *as;
    uint64_t kstack_top;                /* x86: private rsp0 stack */
    int      pid;                       /* -1 = not a process */
    uint64_t uentry;
    uint64_t usp;
} task_struct_t;

/* Number of tasks that ever ran + uptime accessor for reports. */
uint32_t sched_task_count(void);
uint32_t sched_uptime_ticks(void);

/* Preemption: 1 = timer should force this task off the CPU. */
int sched_should_preempt(void);
int      sched_current_index(void);

/*
 * Watchdog: a RUNNING task that has not yielded for `limit`
 * consecutive timer ticks is flagged rogue and killed at its next
 * yield point (cooperative kill; full preemptive kill needs the
 * Stage-6 exception-return switch). limit = 0 disables.
 */
void     sched_watchdog_set(uint32_t limit);

/*
 * Initialize the scheduler and register the calling context as
 * task 0 ("main"). Call once, early in kernel_main.
 */
void sched_init(const char *main_name);

/*
 * Create a task. stack/stack_len describe a caller-owned buffer
 * (16-byte aligned recommended). Returns task index or -1.
 */
int sched_spawn(const char *name, void (*entry)(void),
                void *stack, size_t stack_len);

/*
 * Create a user (EL0) process task: first schedule drops the task
 * into `uentry` at EL0 with SP = `usp`, running in address space
 * `as` (NULL keeps the kernel flat map - used by the legacy demo).
 * The pid field tags the owning process (-1 for plain threads).
 */
int sched_spawn_user_task(const char *name, void *as,
                          uint64_t uentry, uint64_t usp,
                          int pid, void *stack, size_t stack_len);

/* Accessors for the process layer. */
void *sched_task_as(int idx);
int   sched_task_pid(int idx);
void  sched_task_set_pid(int idx, int pid);
void  sched_task_set_kstack(int idx, uint64_t top);

/* EL0 drop-in point of the CURRENTLY RUNNING task (set at spawn
 * time by sched_spawn_user_task and consumed exactly once by the
 * syscall gate's drop trampoline). */
void  sched_current_user_point(uint64_t *uentry, uint64_t *usp);

/* Name of the currently running task (fault diagnostics). */
const char *sched_current_name(void);

/* Consistent snapshot of one task slot for UI/reporting.
 * Returns 0 when the slot holds a task (outputs filled),
 * -1 when the slot is UNUSED. */
int  sched_snapshot(int idx, char *name_out, int *state_out,
                    uint32_t *ticks_out, uint32_t *yields_out);

/* Yield the CPU to the next ready task (round-robin). */
void sched_yield(void);

/* Terminate the calling task; never returns. */
void sched_exit(void) __attribute__((noreturn));

/* Print the task table to the console. */
void sched_report(void);

#endif /* AEOS_SCHED_H */
