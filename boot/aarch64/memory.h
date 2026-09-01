/*
 * AEOS - Memory Management Header
 * Architecture: ARM64 (AArch64)
 *
 * This header provides constants, types, and includes all subsystem
 * headers. New code should include the specific subsystem header
 * directly (pmm.h, vmm.h, heap.h, irq.h, timer.h).
 */

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * Constants
 * ============================================================ */

#define PAGE_SIZE           4096
#define PAGE_SHIFT          12

#define PAGE_ALIGN_UP(a)    (((a) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(a)  ((a) & ~(PAGE_SIZE - 1))

/* ============================================================
 * Memory Map (QEMU virt, 128MB RAM)
 * ============================================================
 *
 * 0x00000000 - 0x08000000  Device I/O
 * 0x40000000 - 0x48000000  RAM (128MB)
 * 0x40080000               Kernel load address (Image text_offset)
 * ============================================================ */

#define KERNEL_BASE     0x40080000
#define RAM_START       0x40000000
#define RAM_SIZE        (128 * 1024 * 1024)
#define RAM_END         (RAM_START + RAM_SIZE)

#define UART_BASE       0x09000000

/* ============================================================
 * Page Table Entry Flags
 * ============================================================ */

#define PTE_VALID       (1UL << 0)
#define PTE_TABLE       (3UL << 0)
#define PTE_BLOCK       (1UL << 0)

#define MT_DEVICE       0
#define MT_NORMAL       1

#define AP_RW_EL1       (0UL << 5)
#define AP_RW_ALL       (1UL << 5)
#define AP_USER_RW      (1UL << 5)          /* EL1 RW + EL0 RW */
#define AP_USER_RO      ((3UL) << 5)        /* EL1 RO + EL0 RO */
#define PTE_AF          (1UL << 10)
#define PTE_PXN         (1UL << 53)         /* no exec at EL1 */
#define PTE_UXN         (1UL << 54)         /* no exec at EL0 */
#define SH_NONE         (0UL << 8)
#define SH_INNER        (3UL << 8)

/* ============================================================
 * Page Table Structures
 * ============================================================ */

typedef uint64_t pte_t;

typedef struct {
    pte_t entries[512];
} page_table_t;

/* ============================================================
 * Utilities
 * ============================================================ */

void memset(void *dest, int val, size_t count);
void memcpy(void *dest, const void *src, size_t count);
int  memcmp(const void *a, const void *b, size_t count);

/* ============================================================
 * Subsystem Headers (backward compatible)
 * ============================================================ */

#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "irq.h"
#include "timer.h"

/* ============================================================
 * AI Engine
 * ============================================================ */
#include "ai_api.h"
#include "ai_core.h"

/* ============================================================
 * CMT Engine (kernel/cmt.h) - Consciousness Manifold Theory
 * ============================================================ */

void     cmt_init(void);
void     cmt_on_tick(void);
void     cmt_force_inference(void);
void     cmt_set_verbose(int enabled);
void     cmt_report(void);
int      cmt_is_healthy(void);
void     cmt_disable(void);
void     cmt_enable(void);
uint32_t cmt_inference_count(void);
uint32_t cmt_error_count(void);
int      cmt_last_action(void);
const char *cmt_action_name(int action);
uint32_t cmt_current_hz(void);
int      cmt_consciousness_level(void);
int      cmt_history_count(void);
uint32_t cmt_history_tick(int idx);
int      cmt_history_action(int idx);
int      cmt_history_level(int idx);
int32_t  cmt_history_energy(int idx);

/* Arch idle wait: scheduler park loops use this. */
#ifndef AEOS_IDLE_WAIT
#define AEOS_IDLE_WAIT() __asm__ volatile ("wfe")
#endif

#endif /* MEMORY_H */
