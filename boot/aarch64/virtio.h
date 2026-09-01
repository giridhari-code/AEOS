/*
 * AEOS - virtio-mmio bus + virtio-blk driver (public API)
 *
 * Polling driver: requests complete by spinning on the used-ring
 * index. No interrupts, no DMA bounce buffers - callers pass
 * identity-mapped RAM (any PMM page) which is DMA-coherent on
 * QEMU's virt machine.
 *
 * Legacy (v1) MMIO interface only: QEMU's virtio-blk-device with a
 * legacy-capable transport exposes VERSION=1 at VIRT_MAGIC+0x4,
 * which keeps the register map and queue layout simple.
 */

#ifndef AEOS_VIRTIO_H
#define AEOS_VIRTIO_H

#include <stdint.h>
#include <stddef.h>

#define VIRTIO_BLK_SECT_SIZE 512UL

/*
 * Scan all 32 virtio-mmio slots (0x0a000000 step 0x200), configure
 * the first block device (device id 2) found, and print a summary.
 * Returns 1 when a block device is ready, 0 otherwise.
 */
int  vblk_init(void);

/* Total capacity reported by the device, in bytes. */
uint64_t vblk_capacity(void);

/* Read/write `count` 512-byte sectors starting at `lba`.
 * Returns 0 on success, -1 on failure. */
int  vblk_read(uint32_t lba, void *buf, uint32_t count);
int  vblk_write(uint32_t lba, const void *buf, uint32_t count);

#endif /* AEOS_VIRTIO_H */
