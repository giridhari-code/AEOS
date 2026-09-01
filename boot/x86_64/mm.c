/*
 * AEOS x86_64 - Memory shims
 * Phase 1: static counters + bump heap so the shell 'mem' command
 * and kernel bookkeeping work. Real PMM comes later.
 */

#include "memory.h"
#include <stdint.h>

#define TOTAL_PAGES (RAM_SIZE / 4096u)
static uint32_t used_pages = 8;      /* kernel image approximation */

extern uint8_t _kernel_end[];
static uint64_t bump_ptr;

uint32_t pmm_get_total_pages(void) { return TOTAL_PAGES; }
uint32_t pmm_get_free_pages(void)  { return TOTAL_PAGES - used_pages; }

void *mm_bump_alloc(uint32_t size)
{
    void *p;

    if (bump_ptr == 0)
        bump_ptr = ((uint64_t)(uintptr_t)_kernel_end + 0xFFF) & ~0xFFFULL;
    if (bump_ptr + size >= RAM_END)
        return 0;
    p = (void *)(uintptr_t)bump_ptr;
    bump_ptr += (size + 0xF) & ~0xFULL;
    return p;
}

/* Heap accounting: the portable code only reads counters. */
static uint32_t heap_used = 1408;

uint32_t heap_get_used(void) { return heap_used; }
uint32_t heap_get_free(void)
{
    return (4u * 1024u * 1024u) - heap_used;   /* 4MB logical heap */
}

size_t heap_get_capacity(void) { return 4u * 1024u * 1024u; }
