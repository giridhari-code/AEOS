/*
 * AEOS - SMP support (Phase 1: dedicated AP workers)
 *
 * The boot core (primary) runs the full kernel - scheduler, shell,
 * AI engines. Secondary cores are powered on via PSCI CPU_ON and
 * each runs smp_main(): a polling loop that pulls jobs from a
 * spinlock-guarded queue. No timer ticks, no preemption on APs -
 * they are pure parallel compute lanes for now.
 *
 * Memory model: all shared state sits in normal inner-shareable
 * RAM; the MMU config marks it shareable and QEMU's TCG keeps
 * cores coherent. Spinlocks use ARMv8.0 LL/SC (ldaxr/stxr).
 */

#include <stdint.h>
#include <stddef.h>

#include "smp.h"
#include "psci.h"
#include "memory.h"
#include "uart.h"

#define MAX_APS      4
#define JOB_QUEUE    16

/* From smp_entry.S */
extern void smp_entry(void);
extern uint8_t ap_stacks[][4096];

/* ---- Spinlock (ARMv8.0 LL/SC) ------------------------------------- */

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

static inline void spin_init(spinlock_t *l) { l->locked = 0; }

static inline void spin_lock(spinlock_t *l)
{
    uint32_t val, res;
    spinlock_t *p = l;

    __asm__ volatile (
        "1:\n"
        "   ldaxr %w0, [%2]\n"
        "   cbnz  %w0, 1b\n"
        "   mov   %w0, #1\n"
        "   stxr  %w1, %w0, [%2]\n"
        "   cbnz  %w1, 1b\n"
        : "=&r"(val), "=&r"(res)
        : "r"(p)
        : "memory");
}

static inline void spin_unlock(spinlock_t *l)
{
    spinlock_t *p = l;

    __asm__ volatile (
        "stlr wzr, [%0]\n"
        : : "r"(p) : "memory");
}

/* ---- State ---------------------------------------------------------- */

typedef struct {
    void (*fn)(uint64_t);
    uint64_t arg;
} job_t;

static job_t jobq[JOB_QUEUE];
static volatile int q_head, q_tail;
static spinlock_t q_lock;

uint8_t ap_stacks[MAX_APS][4096] __attribute__((aligned(16)));
static volatile uint32_t ap_online;             /* bitmask, aff0 idx */
static uint64_t ap_mpidr[MAX_APS];
static spinlock_t uart_lock;

/* Public counters used by demos/tests. */
volatile uint64_t ap_job_count;

static int popcnt(uint32_t v)
{
    int c = 0;

    while (v) { v &= v - 1; c++; }
    return c;
}

/* ---- Primary-side API ----------------------------------------------- */

static void uart_puts_safe(const char *s)
{
    spin_lock(&uart_lock);
    uart_puts(s);
    spin_unlock(&uart_lock);
}


void smp_init(void)
{
    int i;
    int started = 0;

    spin_init(&q_lock);
    spin_init(&uart_lock);

    for (i = 0; i < MAX_APS; i++) {
        /* context id = aff0 value we assign: i+1 */
        if (psci_cpu_on((uint64_t)(uintptr_t)smp_entry, i + 1)
            == 0)
            started++;
    }

    /* Give APs a moment to come online. */
    {
        volatile uint32_t t;

        for (t = 0; t < 200000UL; t++)
            __asm__ volatile ("nop");
    }

    {
        char buf[48];
        int p = 0;
        const char *m1 = "[smp] APs online: ";

        while (m1[p]) { buf[p] = m1[p]; p++; }
        {
            char tmp[4];
            int n = 0;
            uint32_t v = popcnt(ap_online);

            if (!v) tmp[n++] = '0';
            while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
            while (n) buf[p++] = tmp[--n];
        }
        buf[p++] = '/';
        {
            char tmp[4];
            int n = 0;
            uint32_t v = MAX_APS;

            while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
            while (n) buf[p++] = tmp[--n];
        }
        buf[p] = '\0';
        uart_puts_safe(buf);
    }

    if (!started && !ap_online)
        uart_puts_safe("[smp] no secondary cores (boot with -smp N)");
}

int smp_online_count(void)
{
    return popcnt(ap_online);
}

void smp_submit(void (*fn)(uint64_t), uint64_t arg)
{
    int next;

    spin_lock(&q_lock);
    next = (q_tail + 1) % JOB_QUEUE;
    if (next == q_head) {
        spin_unlock(&q_lock);
        return;                         /* queue full: drop */
    }
    jobq[q_tail].fn  = fn;
    jobq[q_tail].arg = arg;
    q_tail = next;
    spin_unlock(&q_lock);

    ap_job_count++;
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("sev");           /* wake idle APs */
}

/* ---- AP-side loop ---------------------------------------------------- */

static int job_pop(job_t *out)
{
    int ok = 0;

    spin_lock(&q_lock);
    if (q_head != q_tail) {
        *out = jobq[q_head];
        q_head = (q_head + 1) % JOB_QUEUE;
        ok = 1;
    }
    spin_unlock(&q_lock);
    return ok;
}

void smp_main(uint64_t ctx)
{
    uint64_t mpidr;
    job_t j;

    __asm__ volatile ("mrs %0, mpidr_el1" : "=r"(mpidr));

    vmm_ap_enable_mmu();

    ap_mpidr[ctx - 1] = mpidr;
    __asm__ volatile ("dsb sy" ::: "memory");
    ap_online |= (1u << (ctx - 1));

    {
        static const char p1[] = "[smp] cpu online: id=";
        static const char p2[] = " mpidr=0x";
        char buf[64];
        int p = 0, n = 0;
        uint64_t v = ctx;
        char tmp[4];

        while (p1[p]) buf[p] = p1[p], p++;
        if (!v) tmp[n++] = '0';
        while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
        while (n) buf[p++] = tmp[--n];
        while (p2[n]) buf[p++] = p2[n], n++;
        for (v = mpidr, n = 15; n >= 0; n--) {
            buf[p + n] = "0123456789abcdef"[v & 0xF];
            v >>= 4;
        }
        p += 16;
        buf[p] = '\0';
        uart_puts_safe(buf);
    }

    for (;;) {
        if (job_pop(&j)) {
            j.fn(j.arg);
            continue;
        }
        AEOS_IDLE_WAIT();
    }
}

/* ---- Reporting -------------------------------------------------------- */

void smp_report(void)
{
    int i;

    uart_puts("  Cores:");
    uart_puts_nolf("    cpu0 (primary) mpidr=0x");
    {
        uint64_t m;
        __asm__ volatile ("mrs %0, mpidr_el1" : "=r"(m));
        uart_hex(m);
    }
    uart_puts("");

    for (i = 0; i < MAX_APS; i++) {
        if (!(ap_online & (1u << i)))
            continue;
        uart_puts_nolf("    cpu");
        uart_dec((unsigned)(i + 1));
        uart_puts_nolf(" (worker)   mpidr=0x");
        uart_hex(ap_mpidr[i]);
        uart_puts("");
    }
    uart_puts_nolf("    jobs queued total: ");
    uart_dec((unsigned)ap_job_count);
    uart_puts("");
}
