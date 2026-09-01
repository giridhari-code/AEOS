/*
 * AEOS - Physical Memory Manager (public API)
 * Architecture: ARM64 (AArch64)
 */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

void      pmm_init(void);
uintptr_t pmm_alloc_page(void);
uintptr_t pmm_alloc_pages(size_t count);
void      pmm_free_page(uintptr_t addr);
void      pmm_free_pages(uintptr_t addr, size_t count);
size_t    pmm_get_total_pages(void);
size_t    pmm_get_free_pages(void);

#endif /* PMM_H */
