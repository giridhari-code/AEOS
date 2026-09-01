/*
 * AEOS - SMP (public API)
 */

#ifndef AEOS_SMP_H
#define AEOS_SMP_H

#include <stdint.h>

/* Power on all secondary cores; call once from the boot core
 * after IRQ bring-up. Safe on -smp 1 (nothing to start). */
void smp_init(void);

int  smp_online_count(void);

/* Queue a job for any idle AP worker. */
void smp_submit(void (*fn)(uint64_t), uint64_t arg);

/* AP main loop - called from smp_entry.S, never returns. */
void smp_main(uint64_t context_id);

/* Shell/report output. */
void smp_report(void);

#endif /* AEOS_SMP_H */
