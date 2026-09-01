/*
 * AEOS - Utilities
 * memset, memcpy, memcmp (freestanding, no libc)
 */

#include "memory.h"

void memset(void *d, int v, size_t n)
{
    unsigned char *p = d;
    unsigned char c = (unsigned char)v;
    while (n--) *p++ = c;
}

void memcpy(void *d, const void *s, size_t n)
{
    unsigned char *dst = d;
    const unsigned char *src = s;
    while (n--) *dst++ = *src++;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *pa = a, *pb = b;
    while (n--) {
        if (*pa != *pb) return *pa - *pb;
        pa++; pb++;
    }
    return 0;
}
