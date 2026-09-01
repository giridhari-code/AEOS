/*
 * AEOS - Interrupt Subsystem (public API)
 * Architecture: ARM64 (AArch64)
 */

#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

/* GIC */
void gic_init(void);
int  gic_acknowledge_irq(void);
void gic_end_of_interrupt(int irq);
void gic_enable_irq(int irq);

/* Tick subscription */
int  tick_subscribe(void (*fn)(void));

/* IRQ masking (PSTATE.DAIF) */
void irq_enable(void);
void irq_disable(void);

/* Init: install vector table, GIC, timer, enable interrupts */
void irq_init(void);

#endif /* IRQ_H */
