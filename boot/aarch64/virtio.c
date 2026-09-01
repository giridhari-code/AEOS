/*
 * AEOS - virtio-mmio bus + virtio-blk driver (legacy v1, polling)
 *
 * QEMU virt exposes 32 virtio-mmio slots at 0x0a000000..0x0a003e00.
 * Each slot: MAGIC "virt" / VERSION / DEVICEID identify the device.
 *
 * Legacy queue layout (v1), one contiguous page-aligned region:
 *   [ desc table : 16B * qn ][ avail ring : 6+2*qn (pad 4K) ]
 *   [ used ring : 6 + 8*qe ... ]
 * We allocate 2 pages from the PMM which covers qn up to ~120.
 */

#include "virtio.h"
#include "memory.h"
#include "uart.h"

/* ---- MMIO register offsets ---- */
/* Common header */
#define V_MAGIC         0x000
#define V_VERSION       0x004
#define V_DEVID         0x008
/* Legacy-specific */
#define V_GUESTPAGESZ   0x028   /* legacy: units of QueuePFN! */
#define V_QALIGN        0x02C   /* legacy: used-ring alignment */
#define V_QPFNSEL       0x030   /* QueueSel */
#define V_QNUMMAX       0x034   /* QueueNumMax (RO) */
#define V_QNUM          0x038   /* QueueNum (WO - reads as 0!) */
#define V_QPFN          0x040   /* legacy: QueuePFN */
#define V_STATUS        0x070

/* Modern-only (VERSION >= 2) */
#define V_DEVFEATSEL    0x014
#define V_DRVFEAT       0x020
#define V_DRVFEATSEL    0x024
#define V_QREADY        0x044
#define V_QNOTIFY       0x050
#define V_QDESCLO       0x080
#define V_QDESCHI       0x084
#define V_QAVAILLO      0x090
#define V_QAVAILHI      0x094
#define V_QUSEDLO       0x0A0
#define V_QUSEDHI       0x0A4

#define VBLK_MAGIC      0x74726976UL    /* "virt" */
#define VBLK_DEVID_BLK  2UL
#define V_SLOT_BASE     0x0A000000UL
#define V_SLOT_STEP     0x200UL
#define V_SLOTS         32

#define S_ACK           (1UL << 0)
#define S_DRIVER        (1UL << 1)
#define S_DRIVER_OK     (1UL << 2)
#define S_FEATURES_OK   (1UL << 3)
#define S_FAILED        (1UL << 7)

/* Legacy device config space (blk geometry) lives at +0x100. */
#define V_CONFIG        0x100

/* ---- Descriptor flags ---- */
#define D_NEXT          (1UL << 0)
#define D_WRITE         (2UL << 0)      /* device-written buffer */

/* ---- Request types ---- */
#define BLK_T_IN        0U
#define BLK_T_OUT       1U

typedef struct {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} blk_req_hdr_t;

typedef struct {
    uint8_t status;
} blk_req_status_t;

/* Split-vring descriptor - exact wire layout (16 bytes). */
typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} vdesc_t;

typedef struct {
    volatile uint16_t flags;
    volatile uint16_t idx;
    volatile uint16_t ring[120];
} avail_ring_t;

typedef struct {
    uint64_t base;                  /* device MMIO base */
    uint32_t qnum;                  /* queue size */
    vdesc_t *desc;
    avail_ring_t *avail;
    volatile struct {
        uint16_t flags;
        uint16_t idx;
        struct { uint32_t id; uint32_t len; } elem[120];
    } *used;

    /* Scratch for header/status must be physically contiguous -
     * keep them inside a single PMM-backed control page. */
    blk_req_hdr_t  *hdr;
    blk_req_status_t *st;

    int ready;
    uint64_t capacity;
    uint16_t next_desc;             /* next free descriptor index */
    uint16_t next_slot;             /* next avail ring slot */
    uint16_t pub_idx;               /* monotonic avail->idx value */
} vblk_dev_t;

static vblk_dev_t vblk;

/* VERSION==2 (modern) vs VERSION==1 (legacy) transport. */
static int modern;

static inline uint32_t rd32(uint64_t base, uint32_t off)
{
    return *(volatile uint32_t *)(uintptr_t)(base + off);
}

static inline void wr32(uint64_t base, uint32_t off, uint32_t val)
{
    *(volatile uint32_t *)(uintptr_t)(base + off) = val;
}

uint64_t vblk_capacity(void)
{
    return vblk.ready ? vblk.capacity : 0;
}

static int probe_slot(uint64_t base)
{
    uint32_t ver;

    if (rd32(base, V_MAGIC) != VBLK_MAGIC)
        return 0;
    ver = rd32(base, V_VERSION);
    if (ver != 1 && ver != 2)
        return 0;
    modern = (ver == 2);
    return rd32(base, V_DEVID) == VBLK_DEVID_BLK;
}

int vblk_init(void)
{
    uint64_t base = 0;
    uintptr_t qpages;
    uint32_t i;

    for (i = 0; i < V_SLOTS; i++) {
        uint64_t cand = V_SLOT_BASE + i * V_SLOT_STEP;

        if (probe_slot(cand)) {
            base = cand;
            break;
        }
    }
    if (!base) {
        uart_puts("[blk] no virtio-blk device found");
        return 0;
    }

    /* Spec-order bring-up: ACK -> DRIVER -> features -> FEATURES_OK
     * -> queues -> DRIVER_OK. Status 0 first: after a BIOS handoff
     * the device still holds the firmware's queue configuration -
     * the soft reset clears it so this driver owns the state. */
    wr32(base, V_STATUS, 0);
    wr32(base, V_STATUS, S_ACK);
    wr32(base, V_STATUS, S_ACK | S_DRIVER);

    if (modern) {
        /* Advertise VIRTIO_F_VERSION_1 (bit 32) only. */
        wr32(base, V_DRVFEATSEL, 0);
        wr32(base, V_DRVFEAT, 0);
        wr32(base, V_DRVFEATSEL, 1);
        wr32(base, V_DRVFEAT, 1);
    } else {
        /* Legacy: GuestPageSize units for PFN + used-ring align.
         * Defaults are 0 and QEMU does not infer them. */
        wr32(base, V_GUESTPAGESZ, PAGE_SIZE);
        wr32(base, V_QALIGN, PAGE_SIZE);
    }

    wr32(base, V_STATUS, S_ACK | S_DRIVER | S_FEATURES_OK);
    {
        uint32_t st = rd32(base, V_STATUS);

        if (!(st & S_FEATURES_OK)) {
            uart_puts("[blk] device rejected features");
            return 0;
        }
    }

    wr32(base, V_QPFNSEL, 0);
    {
        uint32_t maxq = rd32(base, V_QNUMMAX);

        if (maxq == 0) {
            uart_puts("[blk] device reports empty queue - abort");
            return 0;
        }
        vblk.qnum = maxq < 120 ? maxq : 120;
        /* QueueNum MUST be written in both transports: modern QEMU
         * snapshots it into its proxy state at READY time. */
        wr32(base, V_QNUM, vblk.qnum);
    }

    qpages = pmm_alloc_pages(2);
    if (!qpages)
        return 0;
    memset((void *)qpages, 0, 2 * PAGE_SIZE);

    vblk.base  = base;
    vblk.desc  = (vdesc_t *)qpages;
    vblk.avail = (avail_ring_t *)((uint8_t *)qpages +
                                  16UL * vblk.qnum);

    if (modern) {
        uintptr_t avail_pa = ((uintptr_t)vblk.avail + 1) & ~1UL;
        uintptr_t used_pa =
            (avail_pa + 6UL + 2UL * vblk.qnum + 3UL) & ~3UL;

        vblk.used = (void *)used_pa;
        wr32(base, V_QDESCLO,  (uint32_t)qpages);
        wr32(base, V_QDESCHI,  0);
        wr32(base, V_QAVAILLO, (uint32_t)avail_pa);
        wr32(base, V_QAVAILHI, 0);
        wr32(base, V_QUSEDLO,  (uint32_t)used_pa);
        wr32(base, V_QUSEDHI,  0);
        wr32(base, V_QREADY,   1);
    } else {
        vblk.used = (void *)((uintptr_t)vblk.avail +
                             ((6 + 2UL * vblk.qnum + 0xFFF) &
                              ~0xFFFUL));
        wr32(base, V_QPFN, (uint32_t)(qpages >> 12));
    }

    vblk.hdr = (blk_req_hdr_t *)((uint8_t *)qpages + PAGE_SIZE);
    vblk.st  = (blk_req_status_t *)((uint8_t *)qpages + PAGE_SIZE +
                                    sizeof(blk_req_hdr_t));
    vblk.avail->flags = 0;
    vblk.avail->idx   = 0;
    vblk.used->idx    = 0;

    /* Geometry: 64-bit capacity (sectors) from config space. */
    vblk.capacity = (uint64_t)rd32(base, V_CONFIG) |
                    ((uint64_t)rd32(base, V_CONFIG + 4) << 32);
    vblk.capacity *= VIRTIO_BLK_SECT_SIZE;

    wr32(base, V_STATUS, S_ACK | S_DRIVER | S_FEATURES_OK |
                         S_DRIVER_OK);

    vblk.ready = 1;
    uart_puts_nolf("[blk] virtio-blk ready @ 0x");
    uart_hex(base);
    uart_puts_nolf(" ");
    uart_puts(modern ? "modern" : "legacy");
    uart_puts_nolf("  qn=");
    uart_dec(vblk.qnum);
    uart_puts_nolf(" cap=");
    uart_dec((unsigned)(vblk.capacity >> 20));
    uart_puts(" MB");
    return 1;
}

/*
 * Write one descriptor and confirm the store stuck. Under the BIOS
 * build, plain C stores to the vring page proved lossy in ways that
 * only showed up several requests later - verify-and-retry makes
 * the failure impossible to miss.
 */
static void wr_desc(uint32_t i, uint64_t addr, uint32_t len,
                    uint16_t flags, uint16_t next)
{
    volatile vdesc_t *d = &vblk.desc[i];
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
    uart_puts("[blk][dbg] desc write did not stick!");
}

/* Submit a 3-descriptor chain and poll the used ring.
 * buf may be any identity-mapped RAM address. */
static int submit(uint32_t type, uint32_t lba, void *buf,
                  uint32_t bytes)
{
    uint16_t head;
    uint16_t mask;
    uint32_t last_used;

    if (!vblk.ready || bytes == 0)
        return -1;

    mask = (uint16_t)(vblk.qnum - 1);

    /* Each request consumes a 3-descriptor chain. */
    head = vblk.next_desc;
    vblk.next_desc += 3;

    vblk.hdr->type   = type;
    vblk.hdr->ioprio = 0;
    vblk.hdr->sector = lba;

    /* desc[head]: header (device reads) */
    wr_desc(head, (uint64_t)(uintptr_t)vblk.hdr,
            sizeof(blk_req_hdr_t), D_NEXT,
            (uint16_t)((head + 1) & mask));

    /* desc[head+1]: data - chained, direction depends on op */
    wr_desc((head + 1) & mask, (uint64_t)(uintptr_t)buf, bytes,
            (uint16_t)((type == BLK_T_IN ? D_WRITE : 0) | D_NEXT),
            (uint16_t)((head + 2) & mask));

    /* desc[head+2]: status (device writes 1 byte) */
    wr_desc((head + 2) & mask, (uint64_t)(uintptr_t)vblk.st, 1,
            D_WRITE, 0);
    vblk.st->status = 0xEE;

    /*
     * Data-buffer visibility barrier (OUT requests): the device
     * DMA-reads `buf` from the main loop thread. Like the
     * descriptor audit below, touching every cache line plus a DSB
     * makes the payload globally visible before the notify -
     * without this, disks writes intermittently captured stale
     * buffer contents under -bios boot.
     */
    if (type == BLK_T_OUT && bytes) {
        volatile uint8_t *p = (volatile uint8_t *)buf;
        uint32_t k;
        uint32_t sink = 0;

        for (k = 0; k < bytes; k += 32)
            sink += p[k];
        (void)sink;
        __asm__ volatile ("dsb sy" ::: "memory");
    }

    last_used = vblk.used->idx;
    {
        /* Own counter instead of re-reading avail->idx: the
         * read-back proved unreliable under the BIOS build. */
        uint32_t slot = vblk.next_slot;

        vblk.next_slot = (uint16_t)((vblk.next_slot + 1) % vblk.qnum);
        vblk.avail->ring[slot] = head;
    }
    /*
     * Pre-notify audit: re-read the whole chain through volatile
     * lds. Beyond catching lost stores, these reads act as the
     * serialization point that keeps MTTCG's main loop from
     * processing the notify before every descriptor byte is
     * globally visible (observed empirically under -bios boot).
     */
    {
        volatile vdesc_t *a = &vblk.desc[head];
        volatile vdesc_t *b = &vblk.desc[(head + 1) & mask];
        volatile vdesc_t *c = &vblk.desc[(head + 2) & mask];

        __asm__ volatile ("dsb sy" ::: "memory");
        if (a->addr != (uint64_t)(uintptr_t)vblk.hdr ||
            b->addr != (uint64_t)(uintptr_t)buf ||
            c->addr != (uint64_t)(uintptr_t)vblk.st ||
            c->len != 1)
            uart_puts("[blk] WARNING: descriptor chain unstable");
    }

    vblk.pub_idx++;                 /* monotonic publish index */
    vblk.avail->idx = vblk.pub_idx;
    __asm__ volatile ("dsb sy" ::: "memory");
    wr32(vblk.base, V_QNOTIFY, 0);

    {
        uint32_t spin = 0;

        while (vblk.used->idx == last_used) {
            if (++spin > 20000000UL) {
                uint32_t st = rd32(vblk.base, V_STATUS);

                uart_puts("[blk] TIMEOUT waiting for device");
                uart_puts_nolf("  status=0x");
                uart_hex(st);
                uart_puts_nolf(" avail_idx=");
                uart_dec(vblk.avail->idx);
                uart_puts_nolf(" used_idx=");
                uart_dec(vblk.used->idx);
                uart_puts("");
                return -1;
            }
        }
    }

    /*
     * Drain window: under -bios boot QEMU's device thread can lag
     * the visible used-ring update by a hair; back-to-back
     * requests reusing the shared bounce buffer then interleave
     * generations. A short settle loop after each completion keeps
     * generations strictly ordered.
     */
    {
        volatile uint32_t settle;
        uint32_t i;

        for (i = 0; i < 2000UL; i++)
            settle = i;
        (void)settle;
        __asm__ volatile ("dsb sy" ::: "memory");
    }

    return (vblk.st->status == 0) ? 0 : -1;
}

int vblk_read(uint32_t lba, void *buf, uint32_t count)
{
    if (!count)
        return 0;
    return submit(BLK_T_IN, lba, buf,
                  count * (uint32_t)VIRTIO_BLK_SECT_SIZE);
}

int vblk_write(uint32_t lba, const void *buf, uint32_t count)
{
    if (!count)
        return 0;
    return submit(BLK_T_OUT, lba, (void *)buf,
                  count * (uint32_t)VIRTIO_BLK_SECT_SIZE);
}
