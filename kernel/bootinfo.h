/*
 * AEOS - Boot Protocol (shared between stage-2 loader and kernel)
 *
 * The loader fills this struct and hands its address to the kernel
 * in x0. Firmware handoff values (DTB pointer from QEMU -kernel,
 * U-Boot booti, or real firmware) are captured by stage 1 in x0
 * and carried through the loader untouched.
 */

#ifndef AEOS_BOOTINFO_H
#define AEOS_BOOTINFO_H

#include <stdint.h>

#define BOOTINFO_MAGIC   0x41453053UL   /* "AE0S" */
#define LOADER_VERSION   0x00010001UL   /* 1.1 */

#define BI_FLAG_DTB_VALID  (1u << 0)    /* dtb_ptr is a real DTB */
#define BI_FLAG_RAM_MEASURED (1u << 1)   /* ram_size from DTB, not default */
#define BI_FLAG_UART_VALID (1u << 2)    /* uart_base discovered from DTB */

typedef struct {
    uint32_t magic;          /* BOOTINFO_MAGIC */
    uint32_t version;        /* LOADER_VERSION */

    uint64_t ram_base;       /* start of usable RAM */
    uint64_t ram_size;       /* bytes of usable RAM */

    uint64_t uart_base;      /* PL011 base used by loader/kernel */
    uint64_t dtb_ptr;        /* device tree blob address (0 if none) */
    uint32_t dtb_size;       /* DTB totalsize (0 if unknown) */

    uint32_t kernel_size;    /* loaded kernel image size in bytes */

    uint32_t flags;          /* BI_FLAG_* */
    uint32_t reserved;
} bootinfo_t;

#endif /* AEOS_BOOTINFO_H */
