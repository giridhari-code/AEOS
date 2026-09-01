/*
 * AEOS-FS - minimal on-disk filesystem
 *
 * Everything flows through the virtio-blk driver in 512-byte
 * sectors. Two dedicated staging buffers keep DMA contiguous and
 * identity-mapped: one for file payload chunks, one for the
 * directory cache (dirent pointers stay valid inside it).
 */

#include "afs.h"
#include "virtio.h"
#include "memory.h"
#include "uart.h"

#define AFS_MAGIC       0x31534645UL    /* "EFS1" */
#define AFS_SB_LBA      0U
#define AFS_DIR_LBA     1U
#define AFS_DIR_LBAS    1U   /* 16 x 32B dirents fit one sector */

typedef struct {
    char     name[AFS_NAME_LEN];
    uint32_t start_lba;                 /* 0 = free slot */
    uint32_t size;
} afs_dirent_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t total_sectors;
    uint32_t data_start_lba;
} afs_superblock_t;

/* Simple free extent list for reclaimed sectors */
#define AFS_MAX_FREE_EXTENTS 32
typedef struct {
    uint32_t start_lba;
    uint32_t count;
} afs_free_extent_t;

static afs_state_t afs;
static uint32_t afs_base = AFS_BASE_LBA;
static afs_free_extent_t free_extents[AFS_MAX_FREE_EXTENTS];
static uint32_t free_extent_count = 0;

void afs_set_base(uint32_t lba) { afs_base = lba; }
uint32_t afs_get_base(void)     { return afs_base; }

/*
 * Rotating payload buffers: the virtual device may trail the
 * visible ring update slightly; back-to-back requests reusing one
 * buffer could then interleave generations. Four slots make any
 * realistic lag read its own data.
 */
#define IOBUF_SLOTS 4
static uint8_t iobuf[IOBUF_SLOTS][4096] __attribute__((aligned(16)));
static uint8_t diobuf[sizeof(afs_dirent_t) * AFS_MAX_FILES]
    __attribute__((aligned(16)));

/* Local bounded compare (no libc): 1 when equal up to NUL. */
static int fname_eq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return (*a == *b);
}

static int rd_sectors(uint32_t lba, void *dst, uint32_t n)
{
    return vblk_read(afs_base + lba, dst, n);
}

static int wr_sectors(uint32_t lba, const void *src, uint32_t n)
{
    return vblk_write(afs_base + lba, src, n);
}

/* Try to allocate from free extent list; returns LBA or 0 on failure */
static uint32_t alloc_from_free_list(uint32_t nlbas)
{
    uint32_t i;
    for (i = 0; i < free_extent_count; i++) {
        if (free_extents[i].count >= nlbas) {
            uint32_t lba = free_extents[i].start_lba;
            free_extents[i].start_lba += nlbas;
            free_extents[i].count -= nlbas;
            if (free_extents[i].count == 0) {
                /* Remove empty extent from list */
                free_extents[i] = free_extents[free_extent_count - 1];
                free_extent_count--;
            }
            return lba;
        }
    }
    return 0;
}

/* Add extent to free list; returns 0 on success, -1 if list full */
static int add_to_free_list(uint32_t start_lba, uint32_t count)
{
    if (free_extent_count >= AFS_MAX_FREE_EXTENTS)
        return -1;
    free_extents[free_extent_count].start_lba = start_lba;
    free_extents[free_extent_count].count = count;
    free_extent_count++;
    return 0;
}

/* Load the directory into diobuf; NULL on I/O failure. */
static afs_dirent_t *dir_load(void)
{
    if (rd_sectors(AFS_DIR_LBA, diobuf, AFS_DIR_LBAS) != 0)
        return (afs_dirent_t *)0;
    return (afs_dirent_t *)diobuf;
}

static int dir_flush(void)
{
    return wr_sectors(AFS_DIR_LBA, diobuf, AFS_DIR_LBAS);
}

int afs_mount(void)
{
    afs_superblock_t sb;
    afs_dirent_t *d;
    int i;

    if (!vblk_capacity())
        return 0;

    if (rd_sectors(AFS_SB_LBA, iobuf, 1) != 0)
        return 0;
    memcpy(&sb, iobuf, sizeof(sb));
    if (sb.magic != AFS_MAGIC || sb.version != 1)
        return 0;

    afs.total_sectors = sb.total_sectors;
    afs.data_lba      = sb.data_start_lba;
    afs.next_free_lba = sb.data_start_lba;
    afs.mounted       = 1;

    /* Resume bump allocator past the highest existing extent. */
    d = dir_load();
    for (i = 0; d && i < AFS_MAX_FILES; i++) {
        uint32_t end;

        if (!d[i].start_lba)
            continue;
        end = d[i].start_lba +
              (d[i].size + (uint32_t)VIRTIO_BLK_SECT_SIZE - 1) /
                  (uint32_t)VIRTIO_BLK_SECT_SIZE;
        if (end > afs.next_free_lba)
            afs.next_free_lba = end;
    }

    uart_puts_nolf("[fs] AEOS-FS mounted: ");
    uart_dec(afs.total_sectors);
    uart_puts(" sectors");
    return 1;
}

int afs_format(void)
{
    afs_superblock_t sb;

    if (!vblk_capacity())
        return -1;

    memset(iobuf, 0, sizeof(iobuf));
    sb.magic          = AFS_MAGIC;
    sb.version        = 1;
    sb.total_sectors  = (uint32_t)(vblk_capacity() /
                                   VIRTIO_BLK_SECT_SIZE);
    sb.data_start_lba = AFS_SB_LBA + 1 + AFS_DIR_LBAS;
    memcpy(iobuf, &sb, sizeof(sb));
    if (wr_sectors(AFS_SB_LBA, iobuf, 1) != 0)
        return -1;

    memset(diobuf, 0, sizeof(diobuf));
    if (dir_flush() != 0)
        return -1;

    afs.total_sectors = sb.total_sectors;
    afs.data_lba      = sb.data_start_lba;
    afs.next_free_lba = sb.data_start_lba;
    afs.mounted       = 1;

    uart_puts("[fs] AEOS-FS formatted");
    return 0;
}

int afs_file_count(void)
{
    afs_dirent_t *d;
    int i, n = 0;

    if (!afs.mounted)
        return 0;
    d = dir_load();
    if (!d)
        return 0;
    for (i = 0; i < AFS_MAX_FILES; i++)
        if (d[i].start_lba)
            n++;
    return n;
}

void afs_ls(void)
{
    afs_dirent_t *d;
    int i;

    if (!afs.mounted) {
        uart_puts("  no filesystem mounted");
        return;
    }
    d = dir_load();
    if (!d)
        return;
    for (i = 0; i < AFS_MAX_FILES; i++) {
        if (!d[i].start_lba)
            continue;
        uart_puts_nolf("  ");
        uart_puts_nolf(d[i].name);
        uart_puts_nolf("  ");
        uart_dec(d[i].size);
        uart_puts(" bytes");
    }
}

/*
 * Locate `name` in the already-loaded directory (diobuf). When
 * create is set and the name is absent, a fresh zeroed slot is
 * returned with its name filled in (start_lba stays 0 until the
 * caller commits it).
 */
static afs_dirent_t *find_slot(const char *name, int create)
{
    afs_dirent_t *d = (afs_dirent_t *)diobuf;
    int i, free_i = -1;

    for (i = 0; i < AFS_MAX_FILES; i++) {
        if (!d[i].start_lba) {
            if (free_i < 0)
                free_i = i;
            continue;
        }
        if (fname_eq(d[i].name, name))
            return &d[i];
    }
    if (!create || free_i < 0)
        return (afs_dirent_t *)0;
    memset(&d[free_i], 0, sizeof(d[free_i]));
    for (i = 0; name[i] && i < AFS_NAME_LEN - 1; i++)
        d[free_i].name[i] = name[i];
    return &d[free_i];
}

int afs_write(const char *name, const void *data, size_t len)
{
    const uint8_t *src = (const uint8_t *)data;
    size_t off;
    uint32_t lba = 0, nlbas;
    int rewritten = 0;

    if (!afs.mounted || !name || !data || len == 0 ||
        len > 1024UL * 1024UL)
        return -1;

    if (dir_load() == (afs_dirent_t *)0)
        return -1;

    nlbas = (uint32_t)((len + VIRTIO_BLK_SECT_SIZE - 1) /
                       VIRTIO_BLK_SECT_SIZE);

    /* Rewrite in place when an existing extent still fits. */
    {
        afs_dirent_t *existing = find_slot(name, 0);

        if (existing) {
            uint32_t old_nlbas =
                (existing->size + (uint32_t)VIRTIO_BLK_SECT_SIZE -
                 1) / (uint32_t)VIRTIO_BLK_SECT_SIZE;

            if (nlbas <= old_nlbas) {
                lba       = existing->start_lba;
                rewritten = 1;
            }
        }
    }

    if (!rewritten) {
        afs_dirent_t *e = find_slot(name, 1);

        if (!e)
            return -1;                  /* table full */

        /* Try to reuse freed extents first */
        lba = alloc_from_free_list(nlbas);
        if (!lba) {
            /* Fall back to bump allocator */
            lba = afs.next_free_lba;
            if (lba + nlbas > afs.total_sectors)
                return -1;              /* disk full */
            afs.next_free_lba = lba + nlbas;
        }
        e->start_lba = lba;             /* commit slot early */
    }

    for (off = 0; off < len; off += 4096UL) {
        size_t chunk = len - off;
        uint8_t *buf = iobuf[(off / 4096UL) % IOBUF_SLOTS];
        uint32_t sec;

        if (chunk > 4096UL)
            chunk = 4096UL;
        memcpy(buf, src + off, chunk);
        sec = (uint32_t)((chunk + VIRTIO_BLK_SECT_SIZE - 1) /
                         VIRTIO_BLK_SECT_SIZE);
        if (wr_sectors(lba + (uint32_t)(off / VIRTIO_BLK_SECT_SIZE),
                       buf, sec) != 0)
            return -1;
    }

    {
        afs_dirent_t *e = find_slot(name, 0);

        if (!e)
            return -1;
        e->size = (uint32_t)len;
    }
    return dir_flush() == 0 ? (int)len : -1;
}

int afs_read(const char *name, void *buf, size_t max)
{
    afs_dirent_t *e;
    uint8_t *dst = (uint8_t *)buf;
    size_t done = 0;
    uint32_t lba;

    if (!afs.mounted || !name || !buf || max == 0)
        return -1;

    if (dir_load() == (afs_dirent_t *)0)
        return -1;
    e = find_slot(name, 0);
    if (!e)
        return -1;                      /* not found */

    lba = e->start_lba;
    while (done < max && done < e->size) {
        size_t chunk = e->size - done;
        uint8_t *buf = iobuf[((done / 4096UL) + 1) % IOBUF_SLOTS];
        uint32_t sec;

        if (chunk > 4096UL)
            chunk = 4096UL;
        sec = (uint32_t)((chunk + VIRTIO_BLK_SECT_SIZE - 1) /
                         VIRTIO_BLK_SECT_SIZE);
        if (rd_sectors(lba + (uint32_t)(done / VIRTIO_BLK_SECT_SIZE),
                       buf, sec) != 0)
            return -1;
        memcpy(dst + done, buf, chunk);
        done += chunk;
    }
    return (int)done;
}

int afs_delete(const char *name)
{
    afs_dirent_t *e;

    if (!afs.mounted)
        return -1;
    if (dir_load() == (afs_dirent_t *)0)
        return -1;
    e = find_slot(name, 0);
    if (!e)
        return -1;

    /* Track freed extent for reuse */
    if (e->start_lba) {
        uint32_t nlbas = (e->size + (uint32_t)VIRTIO_BLK_SECT_SIZE - 1) /
                         (uint32_t)VIRTIO_BLK_SECT_SIZE;
        if (nlbas > 0)
            add_to_free_list(e->start_lba, nlbas);
    }

    memset(e, 0, sizeof(*e));
    return dir_flush();
}
