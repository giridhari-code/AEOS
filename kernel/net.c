/*
 * AEOS - minimal network stack: Ethernet + ARP + IPv4 + ICMP
 *
 * Single-threaded polling design: net_poll() drains RX, answers
 * ARP for our address and replies to ICMP echo requests (so other
 * machines can ping US), and completes pending pings.
 */

#include "net.h"
#include "vnet.h"
#include "uart.h"
#include "memory.h"

/* ---- addresses (QEMU slirp defaults) ---- */
static const uint8_t our_ip[4]  = { 10, 0, 2, 15 };
static const uint8_t gw_ip[4]   = { 10, 0, 2, 2 };
static const uint8_t bcast[6]   = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };

static uint8_t gw_mac[6];               /* learned via ARP */
static int     have_gw_mac;

static uint16_t icmp_id = 0x4145;       /* "AE" */

/* ---- wire structs (packed by construction) ---- */
typedef struct {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t ethertype;
} eth_hdr_t;

typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sha[6];
    uint8_t  spa[4];
    uint8_t  tha[6];
    uint8_t  tpa[4];
} arp_pkt_t;

typedef struct {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t checksum;
    uint8_t  src[4];
    uint8_t  dst[4];
} ip_hdr_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} icmp_hdr_t;

#define ET_ARP  0x0806
#define ET_IP4  0x0800
#define ARP_REPLY 2
#define IP_PROTO_ICMP 1

static uint16_t cksum(const void *p, size_t len)
{
    const uint8_t *b = p;
    uint32_t sum = 0;
    size_t i;

    for (i = 0; i + 1 < len; i += 2)
        sum += (uint32_t)(b[i] << 8 | b[i + 1]);
    if (i < len)
        sum += (uint32_t)(b[i] << 8);
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

static uint16_t be16(uint16_t v) { return (v >> 8) | (v << 8); }

const char *net_our_ip(void) { return "10.0.2.15"; }

/* ---- transmit helpers ------------------------------------------------ */

static void eth_send(const uint8_t *dst, uint16_t type,
                     const void *payload, size_t plen)
{
    static uint8_t frame[VNET_ETH_MTU];
    eth_hdr_t *eh = (eth_hdr_t *)frame;

    memcpy(eh->dst, dst, 6);
    vnet_get_mac(eh->src);
    eh->ethertype = be16(type);
    if (plen > sizeof(frame) - sizeof(eth_hdr_t))
        plen = sizeof(frame) - sizeof(eth_hdr_t);
    memcpy(frame + sizeof(eth_hdr_t), payload, plen);
    vnet_send(frame, sizeof(eth_hdr_t) + plen);
}

static void arp_send(uint16_t oper, const uint8_t *tha,
                     const uint8_t *tpa)
{
    static arp_pkt_t a;

    memset(&a, 0, sizeof a);
    a.htype = be16(1);
    a.ptype = be16(ET_IP4);
    a.hlen  = 6;
    a.plen  = 4;
    a.oper  = be16(oper);
    vnet_get_mac(a.sha);
    memcpy(a.spa, our_ip, 4);
    memcpy(a.tha, tha, 6);
    memcpy(a.tpa, tpa, 4);
    eth_send(oper == 1 ? bcast : tha, ET_ARP, &a, sizeof a);
}

static void ip_send(const uint8_t *dst, uint8_t proto,
                    const void *body, size_t blen)
{
    static uint8_t pkt[1500];
    ip_hdr_t *ip = (ip_hdr_t *)pkt;

    /* Bounds check: ensure body fits in remaining buffer */
    if (blen > sizeof(pkt) - sizeof *ip)
        blen = sizeof(pkt) - sizeof *ip;

    memset(ip, 0, sizeof *ip);
    ip->ver_ihl   = 0x45;
    ip->total_len = be16((uint16_t)(sizeof *ip + blen));
    ip->id        = be16(0x1234);
    ip->ttl       = 64;
    ip->proto     = proto;
    memcpy(ip->src, our_ip, 4);
    memcpy(ip->dst, dst, 4);
    ip->checksum  = cksum(ip, sizeof *ip);
    memcpy(pkt + sizeof *ip, body, blen);

    if (have_gw_mac)
        eth_send(gw_mac, ET_IP4, pkt, sizeof *ip + blen);
    else
        eth_send(bcast, ET_IP4, pkt, sizeof *ip + blen);
}

static void icmp_reply(const uint8_t *src, const void *req,
                       size_t len)
{
    static uint8_t body[1500];
    icmp_hdr_t *h = (icmp_hdr_t *)body;

    if (len > sizeof body - sizeof *h)
        len = sizeof body - sizeof *h;
    memcpy(body, req, len);
    h->checksum = 0;
    h->checksum = cksum(body, sizeof *h + len);
    ip_send(src, IP_PROTO_ICMP, body, sizeof *h + len);
}

static void icmp_echo_request(uint32_t ip_be, uint16_t seq)
{
    static uint8_t body[40];
    icmp_hdr_t *h = (icmp_hdr_t *)body;

    memset(body, 0, sizeof body);
    h->type     = 8;                     /* request */
    h->id       = icmp_id;
    h->seq      = be16(seq);
    h->checksum = cksum(body, sizeof body);
    ip_send((const uint8_t *)&ip_be, IP_PROTO_ICMP, body,
            sizeof body);
}

/* ---- RX --------------------------------------------------------------- */

static void handle_rx(const uint8_t *frame, size_t len)
{
    eth_hdr_t eh;
    size_t off = sizeof(eth_hdr_t);

    if (len < off + 2)
        return;
    memcpy(&eh, frame, off);

    if (eh.ethertype == be16(ET_ARP) &&
        len >= off + sizeof(arp_pkt_t)) {
        arp_pkt_t a;
        memcpy(&a, frame + off, sizeof a);
        /* who-has OUR ip? -> say mine */
        if (be16(a.oper) == 1 && memcmp(a.tpa, our_ip, 4) == 0)
            arp_send(ARP_REPLY, a.sha, a.spa);
        /* gateway told us its MAC */
        if (be16(a.oper) == 2 && memcmp(a.spa, gw_ip, 4) == 0) {
            memcpy(gw_mac, a.sha, 6);
            have_gw_mac = 1;
        }
        return;
    }

    if (eh.ethertype == be16(ET_IP4) &&
        len >= off + sizeof(ip_hdr_t)) {
        ip_hdr_t ih;
        memcpy(&ih, frame + off, sizeof ih);
        if (ih.proto != IP_PROTO_ICMP)
            return;
        if (memcmp(ih.dst, our_ip, 4) != 0)
            return;
        {
            const unsigned char *body =
                frame + off + sizeof ih;
            size_t blen = len - off - sizeof ih;

            if (blen >= sizeof(icmp_hdr_t) && body[0] == 8)
                icmp_reply(ih.src, body, blen);          /* echo req */
        }
    }
}

/* Drain RX; also resolves pending ping replies. */
void net_poll(void);

void net_poll(void)
{
    static uint8_t buf[VNET_ETH_MTU];
    int n;

    while ((n = vnet_recv(buf, sizeof buf)) > 0)
        handle_rx(buf, (size_t)n);
}

/* ---- public ------------------------------------------------------------ */

extern uint64_t timer_get_ticks(void);

int net_ping(uint32_t ip_be, int count)
{
    int replies = 0, i;

    for (i = 1; i <= count; i++) {
        uint64_t t0, sent_at;
        int got = 0;
        static uint8_t buf[VNET_ETH_MTU];
        int n;

        /* resolve gateway MAC first (slirp answers everything) */
        if (!have_gw_mac) {
            arp_send(1, (const uint8_t *)"\0\0\0\0\0\0", gw_ip);
            for (t0 = timer_get_ticks();
                 timer_get_ticks() - t0 < 30 && !have_gw_mac;)
                net_poll();
            if (!have_gw_mac)
                continue;
        }

        sent_at = timer_get_ticks();
        icmp_echo_request(ip_be, (uint16_t)i);

        for (t0 = timer_get_ticks();
             timer_get_ticks() - t0 < 100 && !got;) {
            net_poll();
            n = vnet_recv(buf, sizeof buf);
            if (n > (int)(sizeof(eth_hdr_t) + sizeof(ip_hdr_t) +
                          sizeof(icmp_hdr_t))) {
                const uint8_t *ic =
                    buf + sizeof(eth_hdr_t) + sizeof(ip_hdr_t);

                if (ic[0] == 0 && ic[7] == (uint8_t)i) {
                    uint64_t dt = timer_get_ticks() - sent_at;

                    uart_puts_nolf("  reply from ");
                    uart_dec((ip_be >> 24) & 0xFF);
                    uart_putc('.');
                    uart_dec((ip_be >> 16) & 0xFF);
                    uart_putc('.');
                    uart_dec((ip_be >> 8) & 0xFF);
                    uart_putc('.');
                    uart_dec(ip_be & 0xFF);
                    uart_puts_nolf(": time ");
                    uart_dec((unsigned)dt * 10);
                    uart_puts(" ms");
                    replies++;
                    got = 1;
                }
            }
        }
        if (!got)
            uart_puts("  request timeout");
    }
    return replies;
}

uint32_t net_parse_ip(const char *s)
{
    uint32_t out = 0;
    int part = 0, parts = 0, ok = 1;

    while (*s) {
        if (*s >= '0' && *s <= '9') {
            part = part * 10 + (*s - '0');
            if (part > 255) { ok = 0; break; }
            s++;
        } else if (*s == '.') {
            out = (out << 8) | (uint32_t)part;
            parts++;
            part = 0;
            s++;
        } else { ok = 0; break; }
    }
    if (!ok || parts != 3) return 0;
    out = (out << 8) | (uint32_t)part;
    /* Reject 0.0.0.0 (invalid) and 255.255.255.255 (broadcast) */
    return (out == 0 || out == 0xFFFFFFFF) ? 0 : out;
}

void net_init(void)
{
    if (!vnet_init())
        return;

    /* announce ourselves + resolve the gateway eagerly */
    arp_send(1, (const uint8_t *)"\0\0\0\0\0\0", gw_ip);
    {
        uint64_t t0;

        for (t0 = timer_get_ticks();
             timer_get_ticks() - t0 < 50 && !have_gw_mac;)
            net_poll();
    }
    uart_puts_nolf("[net] stack up: 10.0.2.15/24 gw 10.0.2.2 ");
    uart_puts(have_gw_mac ? "(gw resolved)" : "(gw pending)");
}
