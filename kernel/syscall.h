/*
 * AEOS - System call gate (public API)
 *
 * User tasks (EL0) reach kernel services exclusively through SVC #0.
 * The syscall number travels in x8, arguments in x0-x2, and the
 * result comes back in x0. This is the only sanctioned user->kernel
 * path - the same boundary discipline the SDK enforces for Python.
 */

#ifndef AEOS_SYSCALL_H
#define AEOS_SYSCALL_H

#include <stdint.h>

#define SYS_PRINT   1    /* x0 = const char* (kernel copies safely) */
#define SYS_TICKS   2    /* -> x0 = timer ticks */
#define SYS_YIELD   3    /* cooperative reschedule */
#define SYS_EXIT    4    /* terminate calling task */
#define SYS_READ    5    /* x0=fd, x1=buf, x2=len -> x0=bytes read */
#define SYS_WRITE   6    /* x0=fd, x1=buf, x2=len -> x0=bytes written */
#define SYS_OPEN    7    /* x0=name -> x0=fd or -1 */
#define SYS_CLOSE   8    /* x0=fd */
#define SYS_IOCTL   9    /* x0=fd, x1=cmd, x2=arg -> x0=result */
#define SYS_CREATE  10   /* x0=name -> x0=0 ok, -1 err */
#define SYS_DELETE  11   /* x0=name -> x0=0 ok, -1 err */
#define SYS_LIST    12   /* x0=buf, x0=max -> x0=count */
#define SYS_TASK_SPAWN 13 /* x0=name, x1=entry -> x0=task_id */
#define SYS_TASK_EXIT  14 /* terminate with code */
#define SYS_AI_REPORT  15 /* print AI+CMT status */
#define SYS_IPC_CREATE 16 /* x0=owner -> x0=queue_id */
#define SYS_IPC_SEND   17 /* x0=qid, x1=data, x2=len, x3=sender -> x0=ok */
#define SYS_IPC_RECV   18 /* x0=qid, x1=buf, x2=len_ptr, x3=sender_ptr -> x0=ok */
#define SYS_IPC_DESTROY 19 /* x0=qid -> x0=ok */
#define SYS_GETPID   20   /* -> x0 = pid (0 = kernel thread) */
#define SYS_WAITPID  21   /* x0=pid, x1=int* code_out -> x0=reaped pid */

/* User-side inline wrappers (EL0). */

static inline long sys_print(const char *s)
{
    register long x0 __asm__("x0") = (long)s;
    register long x8 __asm__("x8") = SYS_PRINT;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

static inline long sys_ticks(void)
{
    register long x8 __asm__("x8") = SYS_TICKS;
    register long x0 __asm__("x0");
    __asm__ volatile ("svc #0" : "=r"(x0) : "r"(x8) : "memory");
    return x0;
}

static inline void sys_yield(void)
{
    register long x8 __asm__("x8") = SYS_YIELD;
    __asm__ volatile ("svc #0" :: "r"(x8) : "memory");
}

static inline void sys_exit(int code)
{
    register long x0 __asm__("x0") = code;
    register long x8 __asm__("x8") = SYS_EXIT;
    __asm__ volatile ("svc #0" :: "r"(x0), "r"(x8) : "memory");
    for (;;)
        __asm__ volatile ("wfe");
}

static inline int sys_getpid(void)
{
    register long x8 __asm__("x8") = SYS_GETPID;
    register long x0 __asm__("x0");
    __asm__ volatile ("svc #0" : "=r"(x0) : "r"(x8) : "memory");
    return (int)x0;
}

static inline int sys_waitpid(int pid, int *code_out)
{
    register long x0 __asm__("x0") = pid;
    register long x1 __asm__("x1") = (long)code_out;
    register long x8 __asm__("x8") = SYS_WAITPID;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)x0;
}

static inline long sys_read(int fd, void *buf, uint32_t len)
{
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = len;
    register long x8 __asm__("x8") = SYS_READ;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
    return x0;
}

static inline long sys_write(int fd, const void *buf, uint32_t len)
{
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = len;
    register long x8 __asm__("x8") = SYS_WRITE;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
    return x0;
}

static inline int sys_open(const char *name)
{
    register long x0 __asm__("x0") = (long)name;
    register long x8 __asm__("x8") = SYS_OPEN;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return (int)x0;
}

static inline void sys_close(int fd)
{
    register long x0 __asm__("x0") = fd;
    register long x8 __asm__("x8") = SYS_CLOSE;
    __asm__ volatile ("svc #0" :: "r"(x0), "r"(x8) : "memory");
}

static inline long sys_ioctl(int fd, uint32_t cmd, uint32_t arg)
{
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = cmd;
    register long x2 __asm__("x2") = arg;
    register long x8 __asm__("x8") = SYS_IOCTL;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
    return x0;
}

static inline int sys_create(const char *name)
{
    register long x0 __asm__("x0") = (long)name;
    register long x8 __asm__("x8") = SYS_CREATE;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return (int)x0;
}

static inline int sys_delete(const char *name)
{
    register long x0 __asm__("x0") = (long)name;
    register long x8 __asm__("x8") = SYS_DELETE;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return (int)x0;
}

static inline int sys_ipc_create(uint32_t owner)
{
    register long x0 __asm__("x0") = owner;
    register long x8 __asm__("x8") = SYS_IPC_CREATE;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return (int)x0;
}

static inline int sys_ipc_send(int qid, const void *data, uint32_t len, uint32_t sender)
{
    register long x0 __asm__("x0") = qid;
    register long x1 __asm__("x1") = (long)data;
    register long x2 __asm__("x2") = len;
    register long x3 __asm__("x3") = sender;
    register long x8 __asm__("x8") = SYS_IPC_SEND;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x8) : "memory");
    return (int)x0;
}

static inline int sys_ipc_recv(int qid, void *buf, uint32_t *len, uint32_t *sender)
{
    register long x0 __asm__("x0") = qid;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = (long)len;
    register long x3 __asm__("x3") = (long)sender;
    register long x8 __asm__("x8") = SYS_IPC_RECV;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x8) : "memory");
    return (int)x0;
}

static inline int sys_ipc_destroy(int qid)
{
    register long x0 __asm__("x0") = qid;
    register long x8 __asm__("x8") = SYS_IPC_DESTROY;
    __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return (int)x0;
}

/* Kernel side: spawn a task that drops to EL0 and runs entry().
 * The task gets its own EL0 stack carved from the given buffer's
 * upper half (lower half stays the kernel stack). */
int sched_spawn_user(const char *name, void (*entry)(void),
                     void *stack, unsigned stack_len);

#endif /* AEOS_SYSCALL_H */
