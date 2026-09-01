/*
 * AEOS - IRQ Dispatch + Fault Handling
 * Architecture: ARM64 (AArch64)
 *
 * IRQ handler dispatches to registered handlers; synchronous
 * exceptions and system errors are dumped to the console and halt.
 *
 * Timer ticks fan out through a small static subscription registry:
 * kernel subsystems (e.g. the AI engine) register a tick callback
 * with tick_subscribe() and are invoked in registration order after
 * the timer driver re-arms. Callbacks must be ISR-safe: no blocking,
 * no allocation, bounded time.
 */

#include "memory.h"
#include "uart.h"

/* External functions */
extern void gic_init(void);
extern int  gic_acknowledge_irq(void);
extern void gic_end_of_interrupt(int irq);
extern void gic_enable_irq(int irq);
extern void timer_init(void);
extern void timer_handler(void);
extern void irq_enable(void);
extern void irq_disable(void);

/* Timer INTID on the GICv2 CPU interface. Empirically determined on
 * QEMU virt (GICv2): the EL1 virtual timer (CNTV) signals PPI ID30
 * here, not the architectural default 27. Verified via GICD_ISPR. */
#define IRQ_TIMER_INTID    30
#define GIC_SPURIOUS_ID    1023

/* ============================================================
 * Tick subscription registry
 * ============================================================ */

#define MAX_TICK_TASKS 4

static void (*tick_tasks[MAX_TICK_TASKS])(void);
static uint32_t tick_task_count;

int tick_subscribe(void (*fn)(void))
{
    if (fn == 0 || tick_task_count >= MAX_TICK_TASKS)
        return -1;
    tick_tasks[tick_task_count++] = fn;
    return 0;
}

static void tick_dispatch(void)
{
    uint32_t i;

    for (i = 0; i < tick_task_count; i++) {
        if (tick_tasks[i])
            tick_tasks[i]();
    }
}

/* ============================================================
 * Exception Handlers (called from exception vector)
 * ============================================================ */

void irq_handler_c(uint64_t *regs)
{
    (void)regs;

    int irq = gic_acknowledge_irq();

    if (irq == GIC_SPURIOUS_ID)
        return;

    if (irq == IRQ_TIMER_INTID) {
        timer_handler();
        tick_dispatch();
    }

    gic_end_of_interrupt(irq);
}

static void fault_halt(const char *kind, uint64_t esr, uint64_t far,
                       uint64_t elr)
{
    irq_disable();
    uart_puts("");
    uart_puts("*** KERNEL FAULT ***");
    uart_puts_nolf("  Class: "); uart_puts(kind);
    uart_puts_nolf("  ESR_EL1: "); uart_hex(esr); uart_puts("");
    uart_puts_nolf("  FAR_EL1: "); uart_hex(far); uart_puts("");
    uart_puts_nolf("  ELR_EL1: "); uart_hex(elr); uart_puts("");
    uart_puts("System halted.");
    for (;;)
        __asm__ volatile ("wfe");
}

void sync_handler_c(uint64_t *regs)
{
    uint64_t esr, far, elr;

    (void)regs;
    __asm__ volatile ("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile ("mrs %0, far_el1" : "=r"(far));
    __asm__ volatile ("mrs %0, elr_el1" : "=r"(elr));
    fault_halt("Synchronous exception", esr, far, elr);
}

/* Synchronous exception taken FROM user mode (lower EL). SVC is
 * the syscall gate; anything else is a user fault -> halt. */
extern void syscall_dispatch(uint64_t *regs);

#define ESR_EC_SVC 0x15

void sync_lower_handler_c(uint64_t *regs)
{
    uint64_t esr, ec;

    __asm__ volatile ("mrs %0, esr_el1" : "=r"(esr));
    ec = (esr >> 26) & 0x3F;

    if (ec == ESR_EC_SVC) {
        syscall_dispatch(regs);
        return;                      /* eret resumes the user task */
    }

    /* Fault diagnostics: identify the offending task before halt. */
    extern int sched_current_index(void);
    extern const char *sched_current_name(void);
    uart_puts_nolf("[fault] task idx=");
    uart_dec((unsigned)sched_current_index());
    uart_puts_nolf(" name='");
    uart_puts_nolf(sched_current_name());
    uart_puts("'");
    fault_halt("User-mode synchronous exception", esr, 0, 0);
}

void fiq_handler_c(uint64_t *regs)
{
    (void)regs;
}

void serror_handler_c(uint64_t *regs)
{
    uint64_t esr;

    (void)regs;
    __asm__ volatile ("mrs %0, esr_el1" : "=r"(esr));
    fault_halt("System Error (SError)", esr, 0, 0);
}


/* ============================================================
 * Interrupt Subsystem Init
 * ============================================================ */

void irq_init(void)
{
    extern uint64_t exception_vector;
    __asm__ volatile ("msr vbar_el1, %0" :: "r" (&exception_vector) : "memory");
    __asm__ volatile ("isb");

    gic_init();
    gic_enable_irq(IRQ_TIMER_INTID);
    timer_init();
    irq_enable();
}
