/*
 * AEOS - GICv2 Driver
 * Architecture: ARM64 (AArch64)
 * Target: QEMU virt machine
 */

#include "memory.h"

/* ============================================================
 * GIC Register Definitions
 * ============================================================ */

#define GICD_BASE       ((uint64_t)0x08000000)
#define GICD_CTLR       (*(volatile uint32_t *)(GICD_BASE + 0x000))
#define GICD_IGROUPR(n) (*(volatile uint32_t *)(GICD_BASE + 0x080 + ((uint64_t)(n)) * 4))
#define GICD_ISENABLER(n) (*(volatile uint32_t *)(GICD_BASE + 0x100 + ((uint64_t)(n)) * 4))
#define GICD_ICENABLER(n) (*(volatile uint32_t *)(GICD_BASE + 0x180 + ((uint64_t)(n)) * 4))
#define GICD_IPRIORITYR(n) (*(volatile uint32_t *)(GICD_BASE + 0x400 + ((uint64_t)(n)) * 4))
#define GICD_ITARGETSR(n)  (*(volatile uint32_t *)(GICD_BASE + 0x800 + ((uint64_t)(n)) * 4))
#define GICD_ICFGR(n)   (*(volatile uint32_t *)(GICD_BASE + 0xC00 + ((uint64_t)(n)) * 4))

#define GICC_BASE       ((uint64_t)0x08010000)
#define GICC_CTLR       (*(volatile uint32_t *)(GICC_BASE + 0x000))
#define GICC_PMR        (*(volatile uint32_t *)(GICC_BASE + 0x004))
#define GICC_IAR        (*(volatile uint32_t *)(GICC_BASE + 0x00C))
#define GICC_EOIR       (*(volatile uint32_t *)(GICC_BASE + 0x010))

/* Register-index arithmetic: each register covers 4 INTIDs
 * (ISENABLER/ICENABLER: 32; IPRIORITYR/ITARGETSR/ICFGR: 4 or 16). */
#define NUM_INTIDS       64   /* SGIs(0-15) + PPIs(16-31) + SPIs(32-63) */
#define NUM_ENABLE_REGS  (NUM_INTIDS / 32)
#define NUM_PRIORITY_REGS (NUM_INTIDS / 4)
#define NUM_TARGET_REGS  (NUM_INTIDS / 4)
#define NUM_CONFIG_REGS  (NUM_INTIDS / 16)

/* ITARGETSR[0..1] (SGIs/PPIs) is read-only on GICv2. */
#define FIRST_SPI_REG    2


void gic_init(void)
{
    int i;

    /* Disable distributor + CPU interface during config */
    GICD_CTLR = 0;
    GICC_CTLR = 0;

    /* Disable all interrupts via ICENABLER (ISENABLER write-0 is a no-op) */
    for (i = 0; i < NUM_ENABLE_REGS; i++)
        GICD_ICENABLER(i) = 0xFFFFFFFFu;

    /* All interrupts Group 0 (secure, handled at EL1) */
    for (i = 0; i < NUM_ENABLE_REGS; i++)
        GICD_IGROUPR(i) = 0;

    /* All interrupts level-triggered */
    for (i = 0; i < NUM_CONFIG_REGS; i++)
        GICD_ICFGR(i) = 0;

    /* Uniform medium priority, one register (4 INTIDs) at a time */
    for (i = 0; i < NUM_PRIORITY_REGS; i++)
        GICD_IPRIORITYR(i) = 0xA0A0A0A0u;

    /* Route SPIs to CPU0; skip read-only SGI/PPI target registers */
    for (i = FIRST_SPI_REG; i < NUM_TARGET_REGS; i++)
        GICD_ITARGETSR(i) = 0x01010101u;

    /* Enable distributor, then CPU interface with full priority range */
    GICD_CTLR = 1;
    GICC_PMR = 0xFF;
    GICC_CTLR = 1;
}

void gic_enable_irq(int irq)
{
    if (irq < 0 || irq >= NUM_INTIDS)
        return;
    GICD_ISENABLER(irq / 32) = (1U << (irq % 32));
}

int gic_acknowledge_irq(void)
{
    return (int)(GICC_IAR & 0x3FF);
}

void gic_end_of_interrupt(int irq)
{
    GICC_EOIR = (uint32_t)irq;
}
