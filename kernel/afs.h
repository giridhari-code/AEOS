/*
 * AEOS-FS - minimal on-disk filesystem (public API)
 *
 * Layout (512-byte sectors):
 *   LBA 0            : superblock
 *   LBA 1 .. 8       : directory (16 entries x 32 bytes)
 *   LBA 9 .. end     : file data, allocated contiguously
 *
 * Directory entry: char name[24]; u32 start_lba; u32 size_bytes.
 * start_lba == 0 marks the slot free. No directories, no
 * permissions, no journal - Phase 2 persistence primitive.
 */

#ifndef AEOS_AFS_H
#define AEOS_AFS_H

#include <stdint.h>
#include <stddef.h>

#define AFS_NAME_LEN     24
#define AFS_BASE_LBA     8192   /* first FS sector on GPT images */
#define AFS_MAX_FILES    16

/* Volume base LBA: AEOS-FS occupies [base, base+total). Disk images
 * reserve the first sectors for MBR/GPT + the raw kernel payload,
 * so every accessor adds this offset. */
void afs_set_base(uint32_t lba);
uint32_t afs_get_base(void);

typedef struct {
    int      mounted;
    uint32_t total_sectors;
    uint32_t data_lba;          /* first data sector */
    uint32_t next_free_lba;     /* bump allocator */
} afs_state_t;

/* Mount an existing FS; returns 1 if a valid superblock was found,
 * 0 if the disk is blank/corrupt (caller may format). */
int  afs_mount(void);

/* Wipe + write a fresh superblock and empty directory. */
int  afs_format(void);

/* Number of files present (fills name buffer pointers when given). */
int  afs_file_count(void);

/* List files to the console ("name size" per line). */
void afs_ls(void);

/*
 * Write `len` bytes as a new file (overwrites same-name entry).
 * Returns bytes written or -1 (no space / bad args).
 */
int  afs_write(const char *name, const void *data, size_t len);

/*
 * Read up to `max` bytes of `name` into buf. Returns bytes read or
 * -1 (not found).
 */
int  afs_read(const char *name, void *buf, size_t max);

int  afs_delete(const char *name);

#endif /* AEOS_AFS_H */
