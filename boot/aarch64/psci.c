/*
 * AEOS - PSCI calls (HVC conduit)
 */

#include <stdint.h>
#include "psci.h"

#define PSCI_FN_SYSTEM_RESET    0x84000009UL
#define PSCI_FN_CPU_ON          0xC4000003UL
#define PSCI_SUCCESS            0UL

void psci_system_reset(void)
{
    register uint64_t x0 __asm__("x0") = PSCI_FN_SYSTEM_RESET;
    register uint64_t x1 __asm__("x1") = 0;

    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("hvc #0"
                      :: "r"(x0), "r"(x1)
                      : "memory");

    /* QEMU resets before returning; park defensively. */
    for (;;)
        __asm__ volatile ("wfe");
}


int psci_cpu_on(uint64_t entry_pa, uint64_t context_id)
{
    register uint64_t x0 __asm__("x0") = PSCI_FN_CPU_ON;
    register uint64_t x1 __asm__("x1") = context_id;
    register uint64_t x2 __asm__("x2") = entry_pa;
    register uint64_t x3 __asm__("x3") = 0;

    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("hvc #0"
                      : "+r"(x0)
                      : "r"(x1), "r"(x2), "r"(x3)
                      : "memory", "cc");
    return (int)x0;
}
