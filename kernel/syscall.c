/*
 * AEOS - System call gate
 *
 * syscall_dispatch() runs inside the lower-EL synchronous exception
 * path with the saved user register frame. The frame layout matches
 * the exception stub: regs[i] = xi, so x8 (syscall number) is
 * regs[8] and the x0 result goes back through regs[0].
 *
 * sched_spawn_user() wraps a user entry function: the task starts
 * at EL1 in a tiny trampoline, switches to SP_EL0, then ERETs into
 * the entry point at EL0.
 */

#include "syscall.h"
#include "sched.h"
#include "memory.h"
#include "uart.h"
#include "device.h"
#include "ramfs.h"
#include "ai_api.h"
#include "cmt.h"
#include "ipc.h"
#include "proc.h"

/* ============================================================
 * User pointer validation
 *
 * Every pointer that originates from a user register (x0-x3) must
 * pass this check before the kernel dereferences it.  Two valid
 * windows exist:
 *   1. The flat identity map (legacy EL0 demo): RAM_START..RAM_END
 *   2. The per-process user VA window: USER_VA_BASE..USER_VA_STACK
 *
 * Returns 1 if the pointer (and the region it accesses) falls
 * entirely within one of these ranges, 0 otherwise.
 * ============================================================ */

static int is_user_ptr(const void *p, uint32_t len)
{
    uintptr_t addr = (uintptr_t)p;

    /* NULL is never valid */
    if (addr == 0)
        return 0;

    /* Region must not wrap around the address space */
    if (addr + len < addr)
        return 0;

    /* Flat identity map (legacy demo tasks) */
    if (addr >= RAM_START && (addr + len) <= RAM_END)
        return 1;

    /* Per-process user VA window */
    if (addr >= USER_VA_BASE && (addr + len) <= USER_VA_STACK)
        return 1;

    return 0;
}

/* Validate a single pointer (no size, just must be non-NULL and in user window). */
static int is_user_ptr_simple(const void *p)
{
    return is_user_ptr(p, 1);
}

/* ============================================================
 * Dispatch
 * ============================================================ */

void syscall_dispatch(uint64_t *regs)
{
    uint64_t num = regs[8];

    switch (num) {
    case SYS_PRINT: {
        /* Copy the string into the console from kernel side; the
         * pointer is user-supplied so bound it defensively.
         * Three valid windows:
         * 1. Kernel text/data (legacy demo tasks share address space)
         * 2. The flat identity map: RAM_START..RAM_END
         * 3. The per-process user window: USER_VA_BASE..USER_VA_STACK */
        const char *s = (const char *)regs[0];
        unsigned n;
        uintptr_t p = (uintptr_t)s;

        /* Allow kernel addresses (legacy tasks) + identity map + user window */
        if (!(p >= 0x40000000UL ||    /* RAM_START or kernel text */
              (p >= USER_VA_BASE && p < USER_VA_STACK))) {
            regs[0] = (uint64_t)-1;  /* -EFAULT */
            break;
        }
        for (n = 0; n < 256 && s[n] != '\0'; n++)
            uart_putc(s[n]);
        regs[0] = n;
        break;
    }
    case SYS_TICKS:
        extern uint64_t timer_get_ticks(void);
        regs[0] = timer_get_ticks();
        break;
    case SYS_YIELD:
        sched_yield();
        break;
    case SYS_EXIT:
        /* x0 = exit code; the process layer records it as a zombie
         * status before parking the calling task. */
        proc_exit((int)regs[0]);    /* never returns */
        break;
    case SYS_GETPID:
        regs[0] = (uint64_t)proc_getpid();
        break;
    case SYS_WAITPID: {
        int code = 0;
        long r = proc_wait((int)regs[0], &code);
        if (r >= 0 && regs[1]) {
            if (!is_user_ptr((void *)regs[1], sizeof(int))) {
                regs[0] = (uint64_t)-1;  /* -EFAULT */
                break;
            }
            *(int *)regs[1] = code;
        }
        regs[0] = (uint64_t)r;
        break;
    }

    /* Device I/O syscalls */
    case SYS_OPEN: {
        const char *name = (const char *)regs[0];
        if (!is_user_ptr_simple(name)) {
            regs[0] = (uint64_t)-1;  /* -EFAULT */
            break;
        }
        device_t *dev = device_open(name);
        if (dev) {
            int fd = proc_fd_alloc(dev);
            regs[0] = (uint64_t)fd;
        } else {
            regs[0] = (uint64_t)-1;
        }
        break;
    }
    case SYS_CLOSE:
        proc_fd_close((int)regs[0]);
        regs[0] = 0;
        break;
    case SYS_READ: {
        if (!is_user_ptr((void *)regs[1], (uint32_t)regs[2])) {
            regs[0] = (uint64_t)-1;  /* -EFAULT */
            break;
        }
        device_t *dev = proc_fd_get((int)regs[0]);
        if (dev) {
            regs[0] = (uint64_t)device_read(dev, (void *)regs[1], (uint32_t)regs[2]);
        } else {
            regs[0] = (uint64_t)-1;
        }
        break;
    }
    case SYS_WRITE: {
        if (!is_user_ptr((const void *)regs[1], (uint32_t)regs[2])) {
            regs[0] = (uint64_t)-1;  /* -EFAULT */
            break;
        }
        device_t *dev = proc_fd_get((int)regs[0]);
        if (dev) {
            regs[0] = (uint64_t)device_write(dev, (const void *)regs[1], (uint32_t)regs[2]);
        } else {
            regs[0] = (uint64_t)-1;
        }
        break;
    }
    case SYS_IOCTL: {
        device_t *dev = proc_fd_get((int)regs[0]);
        if (dev) {
            regs[0] = (uint64_t)device_ioctl(dev, (uint32_t)regs[1], (uint32_t)regs[2]);
        } else {
            regs[0] = (uint64_t)-1;
        }
        break;
    }

    /* RAMFS syscalls */
    case SYS_CREATE:
        if (!is_user_ptr_simple((const char *)regs[0])) {
            regs[0] = (uint64_t)-1;
            break;
        }
        regs[0] = (uint64_t)ramfs_create((const char *)regs[0]);
        break;
    case SYS_DELETE:
        if (!is_user_ptr_simple((const char *)regs[0])) {
            regs[0] = (uint64_t)-1;
            break;
        }
        regs[0] = (uint64_t)ramfs_delete((const char *)regs[0]);
        break;

    /* AI+CMT report */
    case SYS_AI_REPORT:
        ai_report();
        cmt_report();
        regs[0] = 0;
        break;

    /* IPC syscalls */
    case SYS_IPC_CREATE:
        regs[0] = (uint64_t)ipc_create((uint32_t)regs[0]);
        break;
    case SYS_IPC_SEND:
        if ((uint32_t)regs[2] > 0 && !is_user_ptr((const void *)regs[1], (uint32_t)regs[2])) {
            regs[0] = (uint64_t)-1;  /* -EFAULT */
            break;
        }
        regs[0] = (uint64_t)ipc_send((int)regs[0], (const void *)regs[1],
                                      (uint32_t)regs[2], (uint32_t)regs[3]);
        break;
    case SYS_IPC_RECV: {
        void *recv_buf = (void *)regs[1];
        uint32_t *len_ptr = (uint32_t *)regs[2];
        uint32_t *sender_ptr = (uint32_t *)regs[3];

        /* buf is mandatory (we write into it), len and sender are optional outputs */
        if (!is_user_ptr(recv_buf, IPC_MSG_SIZE)) {
            regs[0] = (uint64_t)-1;  /* -EFAULT */
            break;
        }
        if (len_ptr && !is_user_ptr(len_ptr, sizeof(uint32_t))) {
            regs[0] = (uint64_t)-1;
            break;
        }
        if (sender_ptr && !is_user_ptr(sender_ptr, sizeof(uint32_t))) {
            regs[0] = (uint64_t)-1;
            break;
        }
        regs[0] = (uint64_t)ipc_recv((int)regs[0], recv_buf, len_ptr, sender_ptr);
        break;
    }
    case SYS_IPC_DESTROY:
        regs[0] = (uint64_t)ipc_destroy((int)regs[0]);
        break;

    default:
        regs[0] = (uint64_t)-1;  /* -ENOSYS */
        break;
    }
}

/* ============================================================
 * EL0 entry trampoline
 * ============================================================ */

/*
 * Called as the first body of a task created by
 * sched_spawn_user_task(): reads this task's drop-in point and
 * ERETs into it at EL0. Never returns.
 */
void syscall_user_drop(void)
{
    uint64_t entry, sp;

    sched_current_user_point(&entry, &sp);

    /*
     * SPSR: M=EL0t (0b0000), DAIF clear so the timer keeps ticking
     * while user code runs. ELR = entry. The ERET below is the
     * EL1->EL0 boundary crossing.
     */
    __asm__ volatile (
        "msr spsr_el1, %0\n"
        "msr elr_el1, %1\n"
        "msr sp_el0, %2\n"
        "eret\n"
        :: "r"(0x0UL), "r"(entry), "r"(sp)
        : "memory");
}

/*
 * Legacy Phase-0 helper kept for the flat-map demo task in
 * kernel_main: wraps an existing kernel-text function as the EL0
 * entry of a kernel-address-space task.
 */
static uint8_t legacy_stacks[2][1024] __attribute__((aligned(16)));
static int legacy_stack_count;

int sched_spawn_user(const char *name, void (*entry)(void),
                     void *kstack, unsigned kstack_len)
{
    if (legacy_stack_count >= 2)
        return -1;

    uint64_t usp = ((uint64_t)legacy_stacks[legacy_stack_count++] + 1024)
                   & ~0xFUL;

    return sched_spawn_user_task(name, (void *)0, (uint64_t)entry,
                                 usp, -1, kstack, kstack_len);
}
