/*
 * AEOS - ARM Generic Timer Driver
 * Architecture: ARM64 (AArch64)
 * Target: QEMU virt machine
 *
 * Uses EL1 physical timer (CNTP) for periodic interrupts.
 * QEMU virt timer frequency: 24MHz
 */

#include "memory.h"
#include "uart.h"

/*
 * System register encodings for the EL1 VIRTUAL timer (CNTV).
 * CNTV_TVAL_EL1: op0=3, op1=3, CRn=14, CRm=2, op2=0 → s3_3_c14_c2_0
 * CNTV_CTL_EL1:  op0=3, op1=3, CRn=14, CRm=2, op2=1 → s3_3_c14_c2_1
 * CNTFRQ_EL0:    op0=3, op1=3, CRn=14, CRm=0, op2=0 → s3_3_c14_c0_0
 *
 * Why virtual: the physical timer registers (CNTP_*) trap to EL2
 * unless CNTHCTL_EL2.EL1PCEN is set, and QEMU may enter the kernel
 * at EL1 with those gates closed. The virtual timer has no such
 * gate. Its PPI is INTID 27 on the GICv2 CPU interface.
 */
#define CNTV_TVAL_EL1  "s3_3_c14_c2_0"
#define CNTV_CTL_EL1   "s3_3_c14_c2_1"
#define CNTFRQ_EL0     "s3_3_c14_c0_0"

#define TIMER_ENABLE     (1UL << 0)
#define TIMER_FREQ       62500000UL   /* QEMU virt default CNTFRQ */
#define TICK_PER_SEC     100UL
#define TIMER_HZ_MIN     10UL
#define TIMER_HZ_MAX     500UL


static volatile uint64_t timer_ticks = 0;
static void (*timer_callback)(void) = 0;
static uint32_t timer_hz = TICK_PER_SEC;

static void timer_arm(uint32_t hz)
{
    uint64_t freq, ticks;

    __asm__ volatile ("mrs %0, " CNTFRQ_EL0 : "=r" (freq));
    if (freq == 0)
        freq = TIMER_FREQ;
    if (hz == 0)
        hz = TICK_PER_SEC;

    ticks = freq / hz;

    __asm__ volatile ("msr " CNTV_CTL_EL1 ", %0" :: "r" (0UL));
    __asm__ volatile ("msr " CNTV_TVAL_EL1 ", %0" :: "r" (ticks));
    __asm__ volatile ("msr " CNTV_CTL_EL1 ", %0" :: "r" (TIMER_ENABLE));
}


void timer_init(void)
{
    timer_arm(TICK_PER_SEC);
}

void timer_set_hz(uint32_t hz)
{
    if (hz < TIMER_HZ_MIN)
        hz = TIMER_HZ_MIN;
    if (hz > TIMER_HZ_MAX)
        hz = TIMER_HZ_MAX;
    timer_hz = hz;
    timer_arm(timer_hz);
}


void timer_handler(void)
{
    timer_ticks++;

    /* Re-arm at the current policy rate */
    timer_arm(timer_hz);

    if (timer_callback)
        timer_callback();
}


uint64_t timer_get_ticks(void)
{
    return timer_ticks;
}

uint64_t timer_get_ms(void)
{
    return timer_ticks * (1000 / TICK_PER_SEC);
}

uint32_t timer_get_hz(void)
{
    return timer_hz;
}

void timer_set_callback(void (*cb)(void))
{
    timer_callback = cb;
}
