/*
 * AEOS-BIOS - main firmware routine
 *
 * Boot contract: QEMU loads this image into pflash0 (0x00000000)
 * via -bios. The BIOS initializes the console, finds the virtio-blk
 * device, mounts the AEOS-FS volume, loads "kernel.bin" from it to
 * 0x40080000 and jumps there with x0 = 0 (no DTB; the kernel's
 * stage-2 loader falls back to built-in defaults).
 *
 * This makes AEOS fully disk-bootable: no -kernel on the command
 * line, the OS image lives entirely on the virtual hard drive.
 */

#include <stdint.h>

#include "uart.h"
#include "virtio.h"
#include "afs.h"
#include "memory.h"

#define KERNEL_LOAD_ADDR 0x40080000UL
#define KERNEL_MAX_BYTES (4UL * 1024UL * 1024UL)

static void halt(const char *msg)
{
    uart_puts_nolf("[BIOS] FATAL: ");
    uart_puts(msg);
    for (;;)
        __asm__ volatile ("wfe");
}

void bios_main(void);

void bios_main(void)
{
    long size;

    uart_init();
    uart_puts("");
    uart_puts("=========================================");
    uart_puts("  AEOS-BIOS v1.0 - Ajeeb Embodied Firmware");
    uart_puts("=========================================");
    uart_puts("[BIOS] console up, scanning bus...");

    pmm_init();

    if (!vblk_init())
        halt("no virtio-blk device on the bus");

    if (!afs_mount()) {
        uart_puts("[BIOS] blank or foreign disk - formatting...");
        if (afs_format() != 0)
            halt("disk format failed");
    }

    size = afs_read("kernel.bin",
                    (void *)(uintptr_t)KERNEL_LOAD_ADDR,
                    KERNEL_MAX_BYTES);
    if (size <= 0)
        halt("kernel.bin not found on the volume");

    if ((unsigned long)size < 4096)
        halt("kernel.bin too small to be a kernel");

    uart_puts_nolf("[BIOS] kernel.bin: ");
    uart_dec((unsigned)size);
    uart_puts_nolf(" bytes -> 0x");
    uart_hex(KERNEL_LOAD_ADDR);
    uart_puts("");


    uart_puts("[BIOS] jumping to AEOS...");
    uart_puts("");

    __asm__ volatile ("dsb sy" ::: "memory");

    {
        void (*entry)(uint64_t) =
            (void (*)(uint64_t))(uintptr_t)KERNEL_LOAD_ADDR;

        entry(0);                   /* x0: no DTB from BIOS */
    }

    for (;;)
        __asm__ volatile ("wfe");   /* kernels never return */
}
