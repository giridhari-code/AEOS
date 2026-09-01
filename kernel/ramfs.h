/*
 * AEOS - RAM Filesystem (simple flatfs)
 * Files stored in memory, max 32 files, 4KB each
 */
#ifndef RAMFS_H
#define RAMFS_H

#include <stdint.h>

#define RAMFS_MAX_FILES  32
#define RAMFS_MAX_NAME   16
#define RAMFS_MAX_SIZE   4096
#define RAMFS_OK         0
#define RAMFS_ERR_FULL  -1
#define RAMFS_ERR_NOTFOUND -2
#define RAMFS_ERR_PERM  -3
#define RAMFS_ERR_INVAL -4

typedef struct ramfs_file {
    char     name[RAMFS_MAX_NAME];
    uint32_t size;
    uint8_t  in_use;
    uint8_t  data[RAMFS_MAX_SIZE];
} ramfs_file_t;

void     ramfs_init(void);
int      ramfs_create(const char *name);
int      ramfs_write(const char *name, const void *data, uint32_t len, uint32_t offset);
int      ramfs_read(const char *name, void *buf, uint32_t len, uint32_t offset);
int      ramfs_delete(const char *name);
int      ramfs_exists(const char *name);
uint32_t ramfs_size(const char *name);
int      ramfs_list(char names[][RAMFS_MAX_NAME], int max);
void     ramfs_report(void);

#endif /* RAMFS_H */
