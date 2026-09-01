/*
 * AEOS - Kernel Heap (public API)
 * Architecture: ARM64 (AArch64)
 */

#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

void   *kmalloc(size_t size);
void   *kmalloc_aligned(size_t size, size_t alignment);
void    kfree(void *ptr);
size_t  heap_get_used(void);
size_t  heap_get_free(void);
size_t  heap_get_capacity(void);

#endif /* HEAP_H */
