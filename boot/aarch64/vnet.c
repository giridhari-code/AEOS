/*
 * AEOS - virtio-net driver (modern virtio-mmio, polling)
 *
 * Reuses the register/queue knowledge from virtio.c (blk). Two
 * queues, each with its own 2-page descriptor region and a pool of
 * pre-posted RX buffers so the device never has to wait on us.
 */

#include "vnet.h"
#include "memory.h"
#include "uart.h"

/* ---- MMIO (identical layout to blk; modern registers) ---- */
#define V_MAGIC      0x000
#define V_VERSION    0x004
#define V_DEVID      0x008
#define V_DRVFEAT    0x020
#define V_DRVFEATSEL 0x024
#define V_QPFNSEL    0x030
#define V_QNUMMAX    0x034
#define V_QNUM       0x038
#define V_STATUS     0x070
#define V_QREADY     0x044
#define V_QDESCLO    0x080
#define V_QDESCHI    0x084
#define V_QAVAILLO   0x090
#define V_QAVAILHI   0x094
#define V_QUSEDLO    0x0A0
#define V_QUSEDHI    0x0A4
#define V_CFG        0x100

/* legacy-only */
#define V_GUESTPAGESZ 0x028
#define V_QALIGN      0x02C
#define V_QPFN        0x040

#define S_ACK         (1UL << 0)
#define S_DRIVER      (1UL << 1)
#define S_DRIVER_OK   (1UL << 2)
#define S_FEATURES_OK (1UL << 3)

static int legacy;                  /* VERSION==1 transport */

#define VNET_DEVID    1UL              /* network card */
#define V_SLOT_BASE   0x0A000000UL
#define V_SLOT_STEP   0x200UL
#define V_SLOTS       32

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} vdesc_t;

typedef struct {
    volatile uint16_t flags;
    volatile uint16_t idx;
    volatile uint16_t ring[64];
} avail_t;

typedef struct {
    volatile uint16_t flags;
    volatile uint16_t idx;
    struct { uint32_t id, len; } elem[64];
} used_t;

#define QN 32                       /* descriptors per queue */

typedef struct {
    vdesc_t  desc[QN];
    avail_t *avail;
    used_t  *used;
    uint16_t next_desc;
    uint16_t pub_idx;
    uint16_t last_used;
} vq_t;

static struct {
    uint64_t base;
    int ready;
    uint8_t mac[VNET_MAC_LEN];

    /* RX: pre-posted WRITE buffers, one desc each. */
    vq_t rx;
    uint8_t rxbuf[8][2048] __attribute__((aligned(16)));
    int rx_posted;

    /* TX: chains [hdr][data]; hdrs live in their own page. */
    vq_t tx;
    uint8_t txhdr[QN][12] __attribute__((aligned(16)));
} vn;

static inline uint32_t rd32(uint64_t b, uint32_t o)
{
    return *(volatile uint32_t *)(uintptr_t)(b + o);
}

static inline void wr32(uint64_t b, uint32_t o, uint32_t v)
{
    *(volatile uint32_t *)(uintptr_t)(b + o) = v;
}

/*
 * Write + read back one descriptor until it sticks. Under MTTCG
 * the device main-loop races guest stores on first notify - this
 * is the exact medicine that stabilised the blk driver.
 */
static void wr_desc(vq_t *q, uint32_t i, uint64_t addr, uint32_t len,
                    uint16_t flags, uint16_t next)
{
    volatile vdesc_t *d = &q->desc[i];
    uint32_t tries;

    for (tries = 0; tries < 4; tries++) {
        d->addr  = addr;
        d->len   = len;
        d->flags = flags;
        d->next  = next;
        __asm__ volatile ("dsb sy" ::: "memory");
        if (d->addr == addr && d->len == len &&
            d->flags == flags && d->next == next)
            return;
    }
    uart_puts("[net] WARN: descriptor unstable");
}

static void vq_setup(vq_t *q, uint32_t sel)
{
    uint64_t b = vn.base;
    uintptr_t region;
    uint32_t maxq;

    wr32(b, V_QPFNSEL, sel);
    maxq = rd32(b, V_QNUMMAX);
    if (maxq == 0 || maxq > QN)
        maxq = QN < maxq ? QN : maxq;
    if (maxq < 8) {
        uart_puts("[net] queue too small");
        return;
    }
    wr32(b, V_QNUM, maxq);

    region = pmm_alloc_pages(2);
    if (!region)
        return;
    memset((void *)region, 0, 2 * PAGE_SIZE);

    q->avail = (avail_t *)(region + 16UL * QN);
    q->used  = (used_t *)((region + 16UL * QN + sizeof(avail_t) + 3UL)
                          & ~3UL);
    if (rd32(b, V_VERSION) == 1) {
        /* Legacy: the DEVICE computes used-ring placement as
         * align(avail_end, QueueAlign) - our view must match or
         * completions land where we never look. */
        wr32(b, V_QPFN, (uint32_t)(region >> 12));
        q->used = (used_t *)((uintptr_t)region + PAGE_SIZE);
    } else {
        wr32(b, V_QDESCLO,  (uint32_t)region);
        wr32(b, V_QDESCHI,  0);
        wr32(b, V_QAVAILLO, (uint32_t)(uintptr_t)q->avail);
        wr32(b, V_QAVAILHI, 0);
        wr32(b, V_QUSEDLO,  (uint32_t)(uintptr_t)q->used);
        wr32(b, V_QUSEDHI,  0);
        wr32(b, V_QREADY,   1);
    }
}

static void settle(void)
{
    volatile uint32_t s;
    uint32_t i;

    for (i = 0; i < 40000UL; i++)
        s = i;
    (void)s;
    __asm__ volatile ("dsb sy" ::: "memory");
}

static void kick(uint32_t qsel)
{
    /* Pre-kick breathing room lets prior guest stores reach the
     * device main-loop thread; post-kick room lets it work. */
    settle();
    wr32(vn.base, 0x050, qsel);          /* QUEUE_NOTIFY */
    settle();
}

int vnet_init(void)
{
    uint64_t base = 0;
    uint32_t i, ver = 2;

    for (i = 0; i < V_SLOTS; i++) {
        uint64_t cand = V_SLOT_BASE + i * V_SLOT_STEP;

        if (rd32(cand, V_MAGIC) != 0x74726976UL)
            continue;
        ver = rd32(cand, V_VERSION);
        if (ver != 1 && ver != 2)
            continue;
        if (rd32(cand, V_DEVID) == VNET_DEVID) {
            base = cand;
            break;
        }
    }
    if (!base) {
        uart_puts("[net] no virtio-net device found");
        return 0;
    }

    legacy = (rd32(base, V_VERSION) == 1);

    wr32(base, V_STATUS, 0);
    wr32(base, V_STATUS, S_ACK | S_DRIVER);

    if (legacy) {
        /* PFN units + used-ring alignment are NOT inferred by the
         * device - the blk driver learned this the hard way. */
        wr32(base, V_GUESTPAGESZ, PAGE_SIZE);
        wr32(base, V_QALIGN, PAGE_SIZE);
    } else {
        wr32(base, V_DRVFEATSEL, 1);
        wr32(base, V_DRVFEAT, 1);        /* VIRTIO_F_VERSION_1 */
    }
    wr32(base, V_STATUS, S_ACK | S_DRIVER | S_FEATURES_OK);
    {
        uint32_t st = rd32(base, V_STATUS);

        if (!(st & S_FEATURES_OK)) {
            uart_puts_nolf("[net] FEATURES_OK refused, status=0x");
            uart_hex(st);
            uart_puts("");
            return 0;
        }
    }

    vn.base = base;
    memset(&vn.rx, 0, sizeof vn.rx);
    memset(&vn.tx, 0, sizeof vn.tx);

    vq_setup(&vn.tx, 1);                 /* TX first: selects q1  */
    vq_setup(&vn.rx, 0);                 /* then q0               */

    /* MAC from config space. */
    for (i = 0; i < VNET_MAC_LEN; i++)
        vn.mac[i] = *(volatile uint8_t *)(uintptr_t)(base + V_CFG + i);

    /* Device must see DRIVER_OK before we start handing it
     * buffers - otherwise early kicks are silently ignored. */
    wr32(base, V_STATUS, S_ACK | S_DRIVER | S_FEATURES_OK |
                         S_DRIVER_OK);

    vn.ready = 1;

    /* Post receive buffers AFTER the device is fully up. */
    for (i = 0; i < 8; i++) {
        uint32_t d = vn.rx.next_desc++ & (QN - 1);

        wr_desc(&vn.rx, d, (uint64_t)(uintptr_t)vn.rxbuf[i], 2048,
                2 /* WRITE */, 0);
        vn.rx.avail->ring[vn.rx.avail->idx & (QN - 1)] = d;
        __asm__ volatile ("dsb sy" ::: "memory");
        vn.rx.pub_idx++;
        vn.rx.avail->idx = vn.rx.pub_idx;
    }
    kick(0);
    uart_puts_nolf("[net] virtio-net ready @ 0x");
    uart_hex(base);
    {
        char mb[20];
        int p = 0, k;

        for (k = 0; k < VNET_MAC_LEN; k++) {
            static const char hx[] = "0123456789abcdef";

            if (k) mb[p++] = ':';
            mb[p++] = hx[vn.mac[k] >> 4];
            mb[p++] = hx[vn.mac[k] & 0xF];
        }
        mb[p] = '\0';
        uart_puts_nolf(" mac=");
        uart_puts(mb);
    }
    return 1;
}

void vnet_get_mac(uint8_t out[VNET_MAC_LEN])
{
    int i;

    for (i = 0; i < VNET_MAC_LEN; i++)
        out[i] = vn.mac[i];
}

int vnet_send(const void *pkt, size_t len)
{
    static const uint8_t bcast[VNET_MAC_LEN] =
        { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
    uint32_t h, d0, d1;
    uint16_t mask = QN - 1;
    uint32_t spin;

    if (!vn.ready || !pkt || len == 0 || len > VNET_ETH_MTU)
        return -1;
    (void)bcast;

    h  = vn.tx.next_desc;
    d0 = h & mask;
    d1 = (h + 1) & mask;
    vn.tx.next_desc += 2;

    uint32_t use_d1 = 1;
#ifdef VNET_SINGLE_DESC_TEST
    use_d1 = 0;
    wr_desc(&vn.tx, d0, (uint64_t)(uintptr_t)pkt, (uint32_t)len, 0, 0);
#else
    wr_desc(&vn.tx, d0, (uint64_t)(uintptr_t)vn.txhdr[d0], 12,
            1 /*NEXT*/, (uint16_t)d1);
    wr_desc(&vn.tx, d1, (uint64_t)(uintptr_t)pkt, (uint32_t)len,
            0, 0);
#endif
    (void)d1; (void)use_d1;

    vn.tx.avail->ring[vn.tx.avail->idx & mask] = (uint16_t)d0;
    __asm__ volatile ("dsb sy" ::: "memory");
    vn.tx.pub_idx++;
    vn.tx.avail->idx = vn.tx.pub_idx;
    {
        /* visibility audit (same medicine as the blk driver) */
        volatile vdesc_t *a = &vn.tx.desc[d0];
        volatile uint16_t *ai = &vn.tx.avail->idx;

        if (a->addr == 0 || *ai != vn.tx.pub_idx)
            uart_puts("[net] WARN: tx publish unstable");
        __asm__ volatile ("dsb sy" ::: "memory");
    }
    kick(1);

    for (spin = 0; spin < 5000000UL; spin++)
        if (vn.tx.used->idx != vn.tx.last_used)
            break;
    while (vn.tx.last_used != vn.tx.used->idx) {
        vn.tx.last_used++;               /* reclaim lazily */
    }
    return 0;
}

int vnet_recv(void *buf, size_t cap)
{
    uint16_t mask = QN - 1;
    uint32_t d, n;
    int got = 0;

    if (!vn.ready)
        return 0;

    while (vn.rx.last_used != vn.rx.used->idx && got == 0) {
        uint32_t e = vn.rx.last_used % 64;
        uint32_t id = vn.rx.used->elem[e].id;
        uint32_t len = vn.rx.used->elem[e].len;

        len -= 12;                       /* strip virtio net header */
        if (len > cap) len = cap;
        memcpy(buf, vn.rxbuf[id % 8], len);
        got = (int)len;

        /* re-post the buffer */
        d = vn.rx.next_desc++ & mask;
        wr_desc(&vn.rx, d, (uint64_t)(uintptr_t)vn.rxbuf[id % 8],
                2048, 2, 0);
        vn.rx.avail->ring[vn.rx.avail->idx & mask] = (uint16_t)d;
        __asm__ volatile ("dsb sy" ::: "memory");
        vn.rx.pub_idx++;
        vn.rx.avail->idx = vn.rx.pub_idx;
        vn.rx.last_used++;
        (void)n;
        kick(0);
    }
    return got;
}

void vnet_dbg(uint32_t *rx_used, uint32_t *tx_used)
{
    *rx_used = vn.rx.last_used;
    *tx_used = vn.tx.last_used;
}
