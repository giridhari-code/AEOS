/*
 * AEOS - Physical Memory Manager (PMM)
 * Architecture: ARM64 (AArch64)
 *
 * Bitmap allocator with relative frame numbers.
 * frame 0 = 0x40000000, frame 1 = 0x40001000, ...
 */

#include "memory.h"

#define PHYS_TO_FRAME(a) (((a) - RAM_START) >> PAGE_SHIFT)
#define FRAME_TO_PHYS(f) (((f) << PAGE_SHIFT) + RAM_START)

#define BITMAP_SIZE (RAM_SIZE / PAGE_SIZE / 8)
static uint8_t pmm_bitmap[BITMAP_SIZE];

static size_t pmm_total_pages;
static size_t pmm_free_count;

static inline int  pmm_test_bit(size_t f) { return (pmm_bitmap[f/8] >> (f%8)) & 1; }
static inline void pmm_set_bit(size_t f)  { pmm_bitmap[f/8] |=  (1 << (f%8)); }
static inline void pmm_clear_bit(size_t f){ pmm_bitmap[f/8] &= ~(1 << (f%8)); }

void pmm_init(void)
{
    pmm_total_pages = RAM_SIZE / PAGE_SIZE;
    pmm_free_count = 0;

    /* Mark all frames used */
    for (size_t i = 0; i < BITMAP_SIZE; i++)
        pmm_bitmap[i] = 0xFF;

    /* Free usable RAM: after the reserved zone to end of RAM.
     * Reserved: kernel image + page tables (2MB) and the kernel heap
     * (16MB at KERNEL_BASE+16MB) so PMM never hands out heap-owned
     * frames (double-allocation hazard). */
    uintptr_t reserve_end = PAGE_ALIGN_UP(KERNEL_BASE + 18 * 1024 * 1024);
    size_t start = PHYS_TO_FRAME(reserve_end);
    size_t end   = PHYS_TO_FRAME(RAM_END);

    for (size_t f = start; f < end; f++) {
        pmm_clear_bit(f);
        pmm_free_count++;
    }
}

uintptr_t pmm_alloc_page(void)
{
    if (pmm_free_count == 0) return 0;
    size_t total = RAM_SIZE / PAGE_SIZE;
    for (size_t f = 0; f < total; f++) {
        if (!pmm_test_bit(f)) {
            pmm_set_bit(f);
            pmm_free_count--;
            return FRAME_TO_PHYS(f);
        }
    }
    return 0;
}

uintptr_t pmm_alloc_pages(size_t count)
{
    if (count == 0 || pmm_free_count < count) return 0;
    size_t total = RAM_SIZE / PAGE_SIZE;
    for (size_t f = 0; f <= total - count; f++) {
        int ok = 1;
        for (size_t i = 0; i < count; i++)
            if (pmm_test_bit(f + i)) { ok = 0; break; }
        if (ok) {
            for (size_t i = 0; i < count; i++) pmm_set_bit(f + i);
            pmm_free_count -= count;
            return FRAME_TO_PHYS(f);
        }
    }
    return 0;
}

void pmm_free_page(uintptr_t addr)
{
    size_t f = PHYS_TO_FRAME(addr);
    if (f < (RAM_SIZE / PAGE_SIZE) && pmm_test_bit(f)) {
        pmm_clear_bit(f);
        pmm_free_count++;
    }
}

void pmm_free_pages(uintptr_t addr, size_t count)
{
    for (size_t i = 0; i < count; i++)
        pmm_free_page(addr + i * PAGE_SIZE);
}

size_t pmm_get_total_pages(void) { return pmm_total_pages; }
size_t pmm_get_free_pages(void)  { return pmm_free_count; }
