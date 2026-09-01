/*
 * AEOS - Scheduler (cooperative round-robin)
 *
 * Task switching is performed by cpu_switch_to() in
 * boot/aarch64/switch.S, which saves/restores the callee-saved
 * register file plus SP and LR into task_struct_t.cpu (offset 0).
 *
 * A newly spawned task is crafted so its saved LR points at the
 * entry function and its saved SP points at the top of its stack:
 * the first switch-in "returns" straight into the task body.
 */

#include "sched.h"
#include "memory.h"
#include "uart.h"

/* From boot/aarch64/switch.S */
extern void cpu_switch_to(task_struct_t *prev, task_struct_t *next);
extern void task_trampoline(void);

static task_struct_t tasks[MAX_TASKS];
static int current = -1;
static uint32_t uptime;

/* Watchdog: a RUNNING task that has not yielded for this many
 * consecutive ticks is flagged rogue and killed at its next yield
 * point. 0 disables the watchdog. ~6s at 100Hz by default. */
static uint32_t watchdog_limit = 600;

/* Per-arch hook run right before loading the next task's context.
 * x86 uses it to point TSS.rsp0 at the incoming task's private
 * kernel stack so ring-3 interrupt frames never share one buffer. */
#ifdef AEOS_TSS_SWAP
void arch_pre_switch(task_struct_t *next);
#else
static void arch_pre_switch(task_struct_t *next)
{
    (void)next;
}
#endif

/* ============================================================
 * Helpers
 * ============================================================ */

static task_struct_t *cur(void)
{
    return (current >= 0) ? &tasks[current] : (task_struct_t *)0;
}

/*
 * Address-space handoff before a context switch: if the incoming
 * task lives in a different address space, load its page-table
 * base. Kernel threads (as == NULL) share the boot-time flat map.
 */
static void switch_address_space(task_struct_t *prev, task_struct_t *next)
{
#ifdef AEOS_HAS_VMM
    if (prev->as != next->as)
        as_switch((addr_space_t *)next->as);
#else
    (void)prev;
    (void)next;
#endif
}

static int next_ready(int from)
{
    int i;

    for (i = 1; i <= MAX_TASKS; i++) {
        int idx = (from + i) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY)
            return idx;
    }
    return -1;
}

/* ============================================================
 * Timer tick hook (registered via tick_subscribe)
 * ============================================================ */

static void sched_on_tick(void)
{
    task_struct_t *c = cur();

    uptime++;
    if (c && c->state == TASK_RUNNING) {
        c->ticks_used++;

        /* Watchdog: track consecutive ticks without a yield. */
        if (watchdog_limit && current != 0) {   /* never kill main */
            if (++c->starved_ticks >= watchdog_limit &&
                !c->kill_pending) {
                c->kill_pending = 1;
                uart_puts_nolf("[wd] task '");
                uart_puts_nolf(c->name);
                uart_puts("' flagged rogue - kill at next yield");
            }
        }
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

void sched_init(const char *main_name)
{
    int i;

    for (i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_UNUSED;
        tasks[i].name[0] = '\0';
        tasks[i].ticks_used = 0;
        tasks[i].yields = 0;
        tasks[i].starved_ticks = 0;
        tasks[i].kill_pending = 0;
        tasks[i].as = (void *)0;
        tasks[i].kstack_top = 0;
        tasks[i].pid = -1;
        tasks[i].uentry = 0;
        tasks[i].usp = 0;
    }

    /* The boot context becomes task 0. Its registers are live in
     * the CPU; they get captured on its first yield. */
    tasks[0].state = TASK_RUNNING;
    for (i = 0; main_name[i] && i < TASK_NAME_LEN - 1; i++)
        tasks[0].name[i] = main_name[i];
    tasks[0].name[i] = '\0';
    current = 0;

    tick_subscribe(sched_on_tick);
}

static int slot_alloc(void)
{
    int i;

    for (i = 0; i < MAX_TASKS; i++) {
        /* FINISHED slots are reusable. */
        if (tasks[i].state == TASK_UNUSED ||
            tasks[i].state == TASK_FINISHED) {
            return i;
        }
    }
    return -1;
}

static void task_setup(task_struct_t *t, const char *name)
{
    int i;

    for (i = 0; i < 15; i++)
        t->cpu.regs[i] = 0;
    t->state = TASK_READY;
    t->ticks_used = 0;
    t->yields = 0;
    t->starved_ticks = 0;
    t->kill_pending = 0;
    for (i = 0; name[i] && i < TASK_NAME_LEN - 1; i++)
        t->name[i] = name[i];
    t->name[i] = '\0';
}

int sched_spawn(const char *name, void (*entry)(void),
                void *stack, size_t stack_len)
{
    int slot = slot_alloc();
    task_struct_t *t;
    uint64_t sp_top;

    if (slot < 0 || !entry || !stack || stack_len < 256)
        return -1;

    t = &tasks[slot];

    /* Zero the context: fresh tasks start with a clean register
     * file. Only LR (trampoline), SP and x19 (entry) matter for
     * the first switch-in. */
    task_setup(t, name);

    sp_top = ((uint64_t)stack + stack_len) & ~0xFUL;   /* 16-byte align */
    t->cpu.regs[0]  = (uint64_t)entry;        /* x19: read by trampoline */
    t->cpu.regs[11] = (uint64_t)task_trampoline;   /* lr: first entry */
    t->cpu.regs[12] = sp_top;                 /* sp */

    t->as   = (void *)0;
    t->kstack_top = 0;
    t->pid  = -1;
    t->uentry = 0;
    t->usp    = 0;

    return slot;
}

/* Runs at EL1 as the freshly scheduled task's body exactly once:
 * drops to EL0 using this task's uentry/usp and never returns. */
extern void syscall_user_drop(void);

int sched_spawn_user_task(const char *name, void *as,
                          uint64_t uentry, uint64_t usp,
                          int pid, void *stack, size_t stack_len)
{
    int slot = slot_alloc();
    task_struct_t *t;
    uint64_t sp_top;

    if (slot < 0 || !uentry || !usp || !stack || stack_len < 256)
        return -1;

    t = &tasks[slot];
    task_setup(t, name);

    sp_top = ((uint64_t)stack + stack_len) & ~0xFUL;
    t->cpu.regs[0]  = 0;                          /* x19 unused */
#ifdef AEOS_HAS_VMM
    t->cpu.regs[11] = (uint64_t)syscall_user_drop;/* lr: first entry */
#else
    t->cpu.regs[11] = (uint64_t)task_trampoline;
#endif
    t->cpu.regs[12] = sp_top;

    t->as     = as;
    t->kstack_top = 0;
    t->pid    = pid;
    t->uentry = uentry;
    t->usp    = usp;

    return slot;
}

void sched_yield(void)
{
    task_struct_t *prev, *next;
    int nxt;

    prev = cur();
    if (!prev)
        return;

    /* Watchdog verdict: a flagged task never gets the CPU again. */
    if (prev->kill_pending) {
        uart_puts_nolf("[wd] killing task '");
        uart_puts_nolf(prev->name);
        uart_puts("'");
        prev->state = TASK_FINISHED;
        nxt = next_ready(current);
        if (nxt >= 0) {
            next = &tasks[nxt];
            next->state = TASK_RUNNING;
            current = nxt;
            /* Don't leak a masked PSTATE into the next task. */
            irq_enable();
            arch_pre_switch(next);
            switch_address_space(prev, next);
            cpu_switch_to(prev, next);
        }
        irq_enable();
        sched_exit();          /* no ready task: park */
    }

    prev->starved_ticks = 0;

    /*
     * No DAIF masking around the switch: an interrupt landing mid-
     * switch simply runs on the interrupted task's stack and erets
     * back - the callee-saved file being moved belongs to us and is
     * restored symmetrically. Masking here would leak a masked PSTATE
     * into freshly spawned tasks (they enter via the trampoline).
     */
    nxt = next_ready(current);
    if (nxt < 0)
        return;                          /* sole runner: keep going */

    next = &tasks[nxt];
    prev->state = TASK_READY;
    next->state = TASK_RUNNING;
    prev->yields++;
    current = nxt;

    /*
     * A yield may be issued from inside an exception handler (e.g.
     * the EL0 SVC syscall gate) where hardware has masked DAIF.
     * PSTATE is not part of the callee-saved switch context, so
     * without this the mask would leak into the next task and stop
     * the timer dead - freezing every spin_ms() in the system.
     */
    irq_enable();
    arch_pre_switch(next);

    /* Switch register files. When the scheduler eventually switches
     * back to us, cpu_switch_to returns right here. */
    switch_address_space(prev, next);
    cpu_switch_to(prev, next);
}

void sched_exit(void)
{
    task_struct_t *c = cur();
    int nxt;

    if (!c)
        for (;;)
            AEOS_IDLE_WAIT();

    c->state = TASK_FINISHED;

    nxt = next_ready(current);
    if (nxt >= 0) {
        task_struct_t *next = &tasks[nxt];
        next->state = TASK_RUNNING;
        current = nxt;
        /* Don't leak a masked PSTATE into the next task. */
        irq_enable();
        arch_pre_switch(next);
        switch_address_space(c, next);
        cpu_switch_to(c, next);
    }

    /* No other ready task: park forever, but keep interrupts live
     * so the timer and the AI engine continue running. */
    irq_enable();
    for (;;)
        AEOS_IDLE_WAIT();
}

uint32_t sched_task_count(void)
{
    uint32_t n = 0, i;

    for (i = 0; i < MAX_TASKS; i++)
        if (tasks[i].state != TASK_UNUSED)
            n++;
    return n;
}

uint32_t sched_uptime_ticks(void)
{
    return uptime;
}

/* Preemption hook (timer/IRQ context): 1 when the RUNNING task
 * has burned its full quantum AND another task is waiting. The
 * architecture layer redirects the interrupted task through a
 * yield thunk so the switch happens at its own stack. */
int sched_should_preempt(void)
{
    task_struct_t *c = cur();

    if (!c || c->state != TASK_RUNNING)
        return 0;
    if (c->starved_ticks < SCHED_QUANTUM)
        return 0;
    if (next_ready(current) < 0)
        return 0;
    return 1;
}

int sched_current_index(void)
{
    return current;
}

void *sched_task_as(int idx)
{
    if (idx < 0 || idx >= MAX_TASKS)
        return (void *)0;
    return tasks[idx].as;
}

int sched_task_pid(int idx)
{
    if (idx < 0 || idx >= MAX_TASKS)
        return -1;
    return tasks[idx].pid;
}

void sched_task_set_pid(int idx, int pid)
{
    if (idx >= 0 && idx < MAX_TASKS)
        tasks[idx].pid = pid;
}

void sched_task_set_kstack(int idx, uint64_t top)
{
#ifdef AEOS_TSS_SWAP
    extern void arch_tss_set_rsp0(uint64_t v);

    if (idx >= 0 && idx < MAX_TASKS) {
        tasks[idx].kstack_top = top;
        arch_tss_set_rsp0(top);
    }
#else
    (void)idx; (void)top;
#endif
}

void sched_current_user_point(uint64_t *uentry, uint64_t *usp)
{
    task_struct_t *c = cur();

    if (uentry)
        *uentry = c ? c->uentry : 0;
    if (usp)
        *usp = c ? c->usp : 0;
}

const char *sched_current_name(void)
{
    task_struct_t *c = cur();

    return c ? c->name : "?";
}

int sched_snapshot(int idx, char *name_out, int *state_out,
                   uint32_t *ticks_out, uint32_t *yields_out)
{
    task_struct_t *t;
    int i;

    if (idx < 0 || idx >= MAX_TASKS)
        return -1;
    t = &tasks[idx];
    if (t->state == TASK_UNUSED)
        return -1;

    for (i = 0; i < TASK_NAME_LEN - 1 && t->name[i]; i++)
        name_out[i] = t->name[i];
    name_out[i] = '\0';

    if (state_out) *state_out = (int)t->state;
    if (ticks_out) *ticks_out = t->ticks_used;
    if (yields_out) *yields_out = t->yields;
    return 0;
}

void sched_watchdog_set(uint32_t limit)
{
    watchdog_limit = limit;
}

void sched_report(void)
{
    static const char *state_names[] = {
        "UNUSED", "READY", "RUNNING", "FINISHED"
    };
    int i;

    uart_puts("  Task table:");
    for (i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED)
            continue;
        uart_puts_nolf("    [");
        uart_dec((unsigned)i);
        uart_puts_nolf("] ");
        uart_puts_nolf(tasks[i].name);
        uart_puts_nolf("  state=");
        uart_puts(state_names[tasks[i].state]);
        uart_puts_nolf("       ticks=");
        uart_dec(tasks[i].ticks_used);
        uart_puts_nolf("  yields=");
        uart_dec(tasks[i].yields);
        uart_puts("");
    }
}
