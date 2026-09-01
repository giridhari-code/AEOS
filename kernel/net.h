/*
 * AEOS - minimal network stack (public API)
 *
 * Ethernet + ARP + IPv4 + ICMP echo. Static addressing tuned for
 * QEMU user networking (slirp): guest 10.0.2.15/24, gateway
 * 10.0.2.2. Enough to be pingable and to ping out.
 */

#ifndef AEOS_NET_H
#define AEOS_NET_H

#include <stdint.h>
#include <stddef.h>

void net_init(void);                    /* driver + ARP announce   */
int  net_ping(uint32_t ip_be, int count);
    /* returns replies received; prints per-reply RTT */

/* dotted-quad parse: "10.0.2.2" -> BE u32, 0 on error */
uint32_t net_parse_ip(const char *s);

extern const char *net_our_ip(void);

#endif /* AEOS_NET_H */
