/*
 * AEOS - Process Model (Phase 1)
 *
 * Lifecycle:
 *   proc_spawn_image -> READY task + PCB(pid, as, image/stack pages)
 *   SYS_EXIT         -> proc_exit: mark ZOMBIE, park the task forever
 *   proc_wait        -> parent reaps: free AS (user pages + tables),
 *                       release the scheduler slot
 *
 * Everything runs cooperatively inside the syscall gate or kernel
 * tasks; no locks are needed on this single-core build.
 */

#include "proc.h"
#include "sched.h"
#include "memory.h"
#include "uart.h"
#include "device.h"

/* vmm.h comes via memory.h (as_create/as_switch/as_map_page...). */

typedef enum {
    PROC_NONE = 0,
    PROC_ALIVE,
    PROC_ZOMBIE,
} proc_state_t;

typedef struct {
    proc_state_t  state;
    int           pid;
    int           parent;
    int           task_idx;
    void         *as;
    char          name[PROC_NAME_LEN];
    int           exit_code;
    device_t     *fd_table[PROC_FD_MAX];  /* per-process file descriptors */
} proc_t;

static proc_t procs[MAX_PROCS];
static int next_pid = 1;

/* Kernel stacks for user processes' EL1 halves. */
#define PROC_KSTACK_SIZE 1024
static uint8_t kstacks[MAX_PROCS][PROC_KSTACK_SIZE]
    __attribute__((aligned(16)));

/* ============================================================
 * Helpers
 * ============================================================ */

static proc_t *find_pid(int pid)
{
    int i;

    for (i = 0; i < MAX_PROCS; i++)
        if ((procs[i].state == PROC_ALIVE ||
             procs[i].state == PROC_ZOMBIE) && procs[i].pid == pid)
            return &procs[i];
    return (proc_t *)0;
}

static proc_t *find_by_task(int task_idx)
{
    int i;

    for (i = 0; i < MAX_PROCS; i++)
        if (procs[i].state != PROC_NONE && procs[i].task_idx == task_idx)
            return &procs[i];
    return (proc_t *)0;
}

int proc_getpid(void)
{
    proc_t *p = find_by_task(sched_current_index());

    return p ? p->pid : 0;
}

/* ============================================================
 * Creation
 * ============================================================ */

int proc_spawn_image(const char *name, const void *image,
                     size_t size, size_t entry_off)
{
    addr_space_t *as;
    uintptr_t region, chunk;
    uint64_t usp;
    int slot = -1, i, tidx;
    proc_t *p;

    if (!image || size == 0 || entry_off >= size || !name)
        return -1;
    if (size > 0x100000UL)              /* image must fit under 1MB */
        return -1;
    for (i = 0; i < MAX_PROCS; i++)
        if (procs[i].state == PROC_NONE) { slot = i; break; }
    if (slot < 0)
        return -1;
    p = &procs[slot];

    /* Fresh address space with an empty user window. */
    as = as_create();
    if (!as)
        return -1;

    /*
     * One 2MB-aligned physical block backs the whole process:
     * image at +0, stack growing down from the 2MB ceiling. A
     * single RWX block descriptor maps it at USER_VA_BASE.
     */
    chunk = pmm_alloc_pages(1024);      /* 4MB scratch -> 2MB aligned */
    if (!chunk) {
        as_destroy(as);
        return -1;
    }
    region = (chunk + 0x1FFFFFUL) & ~0x1FFFFFUL;
    memset((void *)region, 0, 0x200000UL);
    memcpy((void *)region, image, size);

    if (as_map_block(as, USER_VA_BASE, region, USER_BLOCK_RWX) != 0) {
        pmm_free_pages(chunk, 1024);
        as_destroy(as);
        return -1;
    }

    /* Scheduler task: EL1 half runs on a private kstack; its very
     * first schedule drops straight to EL0 at USER_VA_BASE+entry. */
    usp = (USER_VA_STACK - 16) & ~0xFUL;
    tidx = sched_spawn_user_task(name, as,
                                 USER_VA_BASE + entry_off,
                                 usp,
                                 next_pid,
                                 kstacks[slot], PROC_KSTACK_SIZE);
    if (tidx < 0) {
        pmm_free_pages(chunk, 1024);
        as_destroy(as);
        return -1;
    }

    p->state    = PROC_ALIVE;
    p->pid      = next_pid++;
    p->parent   = 0;                    /* Phase 1: children of kernel */
    p->task_idx = tidx;
    p->as       = as;
    p->exit_code = 0;
    for (i = 0; name[i] && i < PROC_NAME_LEN - 1; i++)
        p->name[i] = name[i];
    p->name[i] = '\0';

    /* Initialize per-process file descriptor table */
    for (i = 0; i < PROC_FD_MAX; i++)
        p->fd_table[i] = (device_t *)0;

    uart_puts_nolf("[proc] spawned '");
    uart_puts_nolf(p->name);
    uart_puts_nolf("' pid=");
    uart_dec((unsigned)p->pid);
    uart_puts_nolf(" task=");
    uart_dec((unsigned)tidx);
    uart_puts("");

    return p->pid;
}

/* ============================================================
 * Exit / wait
 * ============================================================ */

/*
 * Zombie transition. The task itself parks forever afterwards via
 * sched_exit(); its resources stay booked until the parent reaps.
 */
void proc_exit(int code)
{
    proc_t *p = find_by_task(sched_current_index());

    if (!p) {
        sched_exit();                   /* not a process: plain exit */
        __builtin_unreachable();        /* silence warning, never returns */
    }

    p->exit_code = code;
    p->state     = PROC_ZOMBIE;

    uart_puts_nolf("[proc] '");
    uart_puts_nolf(p->name);
    uart_puts_nolf("' exited code=");
    uart_dec((unsigned)(code < 0 ? 0 : code));
    uart_puts("");

    sched_exit();                       /* never returns */
}

int proc_wait(int pid, int *code_out)
{
    proc_t *p;
    int reaped_task;

    for (;;) {
        p = find_pid(pid);
        if (!p)
            return -1;
        if (p->state == PROC_ZOMBIE) {
            reaped_task = p->task_idx;
            if (code_out)
                *code_out = p->exit_code;

            /* Release machine resources now that nobody observes
             * the status anymore. */
            as_destroy((addr_space_t *)p->as);
            p->as       = (void *)0;
            p->state    = PROC_NONE;
            p->task_idx = -1;
            sched_task_set_pid(reaped_task, -1);
            return pid;
        }
        sched_yield();
    }
}

/* ============================================================
 * Reporting
 * ============================================================ */

void proc_report(void)
{
    static const char *state_names[] = { "-", "ALIVE", "ZOMBIE" };
    int i;

    uart_puts("  Process table:");
    for (i = 0; i < MAX_PROCS; i++) {
        if (procs[i].state == PROC_NONE)
            continue;
        uart_puts_nolf("    pid=");
        uart_dec((unsigned)procs[i].pid);
        uart_puts_nolf(" '");
        uart_puts_nolf(procs[i].name);
        uart_puts_nolf("' ");
        uart_puts_nolf(state_names[procs[i].state]);
        uart_puts_nolf(" parent=");
        uart_dec((unsigned)procs[i].parent);
        uart_puts("");
    }
}

/* ============================================================
 * Per-process file descriptor operations
 * ============================================================ */

/* Global FD table for kernel threads (pid 0) only */
static device_t *kernel_fd_table[PROC_FD_MAX];

static proc_t *find_current_proc(void)
{
    return find_by_task(sched_current_index());
}

int proc_fd_alloc(device_t *dev)
{
    proc_t *p = find_current_proc();
    device_t **fdt = p ? p->fd_table : kernel_fd_table;
    int i;

    for (i = 0; i < PROC_FD_MAX; i++) {
        if (!fdt[i]) {
            fdt[i] = dev;
            return i;
        }
    }
    return -1;
}

device_t *proc_fd_get(int fd)
{
    proc_t *p = find_current_proc();
    device_t **fdt = p ? p->fd_table : kernel_fd_table;

    if (fd < 0 || fd >= PROC_FD_MAX)
        return (device_t *)0;
    return fdt[fd];
}

void proc_fd_close(int fd)
{
    proc_t *p = find_current_proc();
    device_t **fdt = p ? p->fd_table : kernel_fd_table;

    if (fd >= 0 && fd < PROC_FD_MAX)
        fdt[fd] = (device_t *)0;
}
