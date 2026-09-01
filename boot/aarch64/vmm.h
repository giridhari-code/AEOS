/*
 * AEOS - Virtual Memory Manager (public API)
 * Architecture: ARM64 (AArch64)
 */

#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

/*
 * Address space handle. Opaque to callers; the kernel address space
 * is represented by NULL and always maps the flat identity image.
 */
typedef struct addr_space addr_space_t;

/* User VA window. Must sit in its own top-level PGD slot (>= 2^39)
 * because every kernel mapping lives below that boundary and the
 * kernel descriptors are shared by value into each address space.
 * USER_VA_BASE = 2^39 -> PGD index 1. */
#define USER_VA_BASE   0x8000000000UL          /* 2^39: PGD index 1 */
#define USER_VA_STACK  (USER_VA_BASE + 0x200000UL)

void     vmm_init(void);
void     vmm_enable_mmu(void);
void     vmm_ap_enable_mmu(void);   /* secondary cores */
uint64_t vmm_get_phys(uint64_t virt);
void     vmm_flush_tlb(uint64_t va);
void     vmm_flush_tlb_all(void);

/* --- Per-process address spaces ----------------------------------- */

/*
 * Create an address space: kernel identity mappings are shared by
 * value-copied top-level entries; the user window starts empty.
 * Returns NULL when page tables cannot be allocated.
 */
addr_space_t *as_create(void);

/* Release every table + mapped user page back to the PMM. */
void as_destroy(addr_space_t *as);

/* Load `as` (NULL = kernel AS) into TTBR0_EL1 and flush the TLB.
 * Safe to call with as == currently-active space (no-op-ish). */
void as_switch(addr_space_t *as);

/* Active-space query used to skip redundant TTBR0 writes. */
addr_space_t *as_current(void);

/*
 * Map one 2MB block into the user window at `va` (2MB-aligned)
 * backed by physical `pa` (2MB-aligned). Returns 0/-1.
 *
 * NOTE: Phase 1 maps user memory with L2 block descriptors; the
 * finer-grained 4KB L3 path hits a QEMU TCG walk quirk and is
 * disabled until root-caused.
 */
int as_map_block(addr_space_t *as, uint64_t va, uintptr_t pa,
                 uint64_t attrs);

/* Attribute presets. */
#define USER_PAGE_RX   (PTE_AF | (MT_NORMAL << 2) | SH_INNER | \
                        AP_USER_RO | PTE_PXN)
#define USER_PAGE_RW   (PTE_AF | (MT_NORMAL << 2) | SH_INNER | \
                        AP_USER_RW | PTE_UXN | PTE_PXN)
/* Phase 1: code+data+stack share a single 2MB block, so we must
 * allow both write (for data/stack) and execute (for code) at EL0.
 * This violates W^X and will be fixed when the linker script
 * separates code into its own RX-only mapping. */
#define USER_BLOCK_RWX (PTE_AF | (MT_NORMAL << 2) | SH_INNER | \
                        AP_USER_RW)

#endif /* VMM_H */
