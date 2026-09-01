/*
 * AEOS - Kernel Heap (Bump Allocator)
 * Architecture: ARM64 (AArch64)
 *
 * Simple bump allocator at 0x41000000 (16MB).
 * VMM already identity-mapped all RAM, so no per-page mapping needed.
 */

#include "memory.h"

#define HEAP_START  (KERNEL_BASE + 16 * 1024 * 1024)  /* 0x41000000 */
#define HEAP_SIZE   (16 * 1024 * 1024)                 /* 16MB */
#define HEAP_END    (HEAP_START + HEAP_SIZE)

static uintptr_t heap_ptr;
static size_t    heap_used;

void heap_init(void)
{
    heap_ptr  = HEAP_START;
    heap_used = 0;
    memset((void *)HEAP_START, 0, HEAP_SIZE);
}

void *kmalloc(size_t size)
{
    if (size == 0) return 0;
    size = (size + 7) & ~7;  /* 8-byte align */
    if (heap_ptr + size > HEAP_END) return 0;
    void *p = (void *)heap_ptr;
    heap_ptr += size;
    heap_used += size;
    return p;
}

void *kmalloc_aligned(size_t size, size_t align)
{
    if (size == 0 || align == 0 || (align & (align - 1))) return 0;
    heap_ptr = (heap_ptr + align - 1) & ~(align - 1);
    return kmalloc(size);
}

void kfree(void *p) { (void)p; }

size_t heap_get_used(void) { return heap_used; }
size_t heap_get_free(void) { return HEAP_END - heap_ptr; }
size_t heap_get_capacity(void) { return HEAP_SIZE; }
