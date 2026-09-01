/*
 * AEOS x86_64 - platform stubs
 *
 * The shared subsystems (shell) reference a few Phase-2 facilities
 * that currently exist only on the aarch64 port: the virtio-blk
 * backed filesystem, PSCI power ops and the process table report.
 * Neutral implementations keep the shared sources linkable until
 * those features are ported.
 */

#include <stdint.h>
#include <stddef.h>

#include "afs.h"
#include "psci.h"
#include "sched.h"
#include "uart.h"

/* --- afs.h --------------------------------------------------------- */

int  afs_mount(void)            { return 0; }
int  afs_format(void)           { return -1; }
int  afs_file_count(void)       { return 0; }
void afs_ls(void)               { }

int  afs_write(const char *name, const void *data, size_t len)
{
    (void)name; (void)data; (void)len;
    return -1;
}

int  afs_read(const char *name, void *buf, size_t max)
{
    (void)name; (void)buf; (void)max;
    return -1;
}

int  afs_delete(const char *name)
{
    (void)name;
    return -1;
}

/* --- psci.h -------------------------------------------------------- */

void psci_system_reset(void)
{
    for (;;)
        __asm__ volatile ("hlt");
}

/* --- proc.h -------------------------------------------------------- */

void proc_report(void)
{
    uart_puts("  process table: not available on this port");
}


/* ---- TSS / per-task kernel stacks (sched AEOS_TSS_SWAP hook) ---- */

void tss_set_rsp0(uint64_t v);

void arch_tss_set_rsp0(uint64_t v)
{
    tss_set_rsp0(v);
}


/* sched AEOS_TSS_SWAP hook: point TSS.rsp0 at the incoming task's
 * private kernel stack (user tasks register one at spawn time). */
#include "sched.h"

void arch_pre_switch(task_struct_t *next)
{
    if (next->kstack_top)
        tss_set_rsp0(next->kstack_top);
}
