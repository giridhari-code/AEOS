/*
 * AEOS - PSCI (Power State Coordination Interface)
 *
 * QEMU's virt machine exposes PSCI 0.2+ over the HVC conduit
 * (pin it with -machine virt,psci-conduit=hvc). SYSTEM_RESET
 * re-enters the firmware: for us that means AEOS-BIOS reruns and
 * boots whatever kernel image is on disk - the foundation of the
 * self-update flow.
 */

#ifndef AEOS_PSCI_H
#define AEOS_PSCI_H

/* Reset the system; control goes back to the BIOS. Never returns. */
void psci_system_reset(void) __attribute__((noreturn));

/* Power a secondary core on: it starts at `entry` (PA) with x0 =
 * context_id. Returns 0 on success (PSCI SUCCESS). */
int  psci_cpu_on(uint64_t entry_pa, uint64_t context_id);

#endif /* AEOS_PSCI_H */
