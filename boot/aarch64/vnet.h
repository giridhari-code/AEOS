/*
 * AEOS - virtio-net driver (modern virtio-mmio, public API)
 *
 * Same transport as the blk driver (see virtio.c): two queues -
 * RX (0) and TX (1). Pre-posted receive buffers; transmit chains
 * of [net header][packet]. Polling, no interrupts.
 */

#ifndef AEOS_VNET_H
#define AEOS_VNET_H

#include <stdint.h>
#include <stddef.h>

#define VNET_ETH_MTU 1500
#define VNET_MAC_LEN 6

/* Probe + init both queues. Returns 1 when a NIC is live. */
int  vnet_init(void);

/* Device MAC (valid after vnet_init). */
void vnet_get_mac(uint8_t out[VNET_MAC_LEN]);

/* Submit one outgoing packet (copied). 0 = accepted. */
int  vnet_send(const void *pkt, size_t len);

/*
 * Poll for one received packet into buf (cap = buffer size).
 * Returns length (>0) or 0 when nothing pending.
 */
int  vnet_recv(void *buf, size_t cap);

/* bring-up diagnostics */
void vnet_dbg(uint32_t *rx_used, uint32_t *tx_used);

#endif /* AEOS_VNET_H */
