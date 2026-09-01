/*
 * AEOS - RAM Filesystem Implementation
 */
#include "ramfs.h"
#include "uart.h"

static ramfs_file_t files[RAMFS_MAX_FILES];
static uint32_t file_count = 0;

void ramfs_init(void) {
    uint32_t i;
    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        files[i].in_use = 0;
        files[i].size = 0;
    }
    file_count = 0;
    uart_puts("  RAMFS initialized (32 files, 4KB each)");
}

static ramfs_file_t *find_file(const char *name) {
    uint32_t i;
    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].in_use) continue;
        uint32_t j;
        for (j = 0; j < RAMFS_MAX_NAME; j++) {
            if (files[i].name[j] != name[j]) break;
        }
        if (j == RAMFS_MAX_NAME || files[i].name[j] == name[j])
            return &files[i];
    }
    return 0;
}

int ramfs_create(const char *name) {
    uint32_t i;
    if (find_file(name)) return RAMFS_OK;

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].in_use) {
            uint32_t j;
            for (j = 0; j < RAMFS_MAX_NAME && name[j]; j++)
                files[i].name[j] = name[j];
            for (; j < RAMFS_MAX_NAME; j++)
                files[i].name[j] = 0;
            files[i].size = 0;
            files[i].in_use = 1;
            file_count++;
            return RAMFS_OK;
        }
    }
    return RAMFS_ERR_FULL;
}

int ramfs_write(const char *name, const void *data, uint32_t len, uint32_t offset) {
    ramfs_file_t *f = find_file(name);
    if (!f) return RAMFS_ERR_NOTFOUND;
    if (offset >= RAMFS_MAX_SIZE) return RAMFS_ERR_INVAL;

    uint32_t i;
    const uint8_t *src = (const uint8_t *)data;
    uint32_t remaining = RAMFS_MAX_SIZE - offset;
    uint32_t copy_len = (len > remaining) ? remaining : len;
    uint32_t end = offset + copy_len;

    for (i = offset; i < end; i++)
        f->data[i] = src[i - offset];

    if (f->size < end) f->size = end;
    return (int)copy_len;
}

int ramfs_read(const char *name, void *buf, uint32_t len, uint32_t offset) {
    ramfs_file_t *f = find_file(name);
    if (!f) return RAMFS_ERR_NOTFOUND;

    uint32_t i;
    uint8_t *dst = (uint8_t *)buf;
    uint32_t end = offset + len;
    if (end > f->size) end = f->size;
    if (offset >= f->size) return 0;

    for (i = offset; i < end; i++)
        dst[i - offset] = f->data[i];

    return (int)(end - offset);
}

int ramfs_delete(const char *name) {
    ramfs_file_t *f = find_file(name);
    if (!f) return RAMFS_ERR_NOTFOUND;
    f->in_use = 0;
    f->size = 0;
    file_count--;
    return RAMFS_OK;
}

int ramfs_exists(const char *name) {
    return find_file(name) != 0;
}

uint32_t ramfs_size(const char *name) {
    ramfs_file_t *f = find_file(name);
    return f ? f->size : 0;
}

int ramfs_list(char names[][RAMFS_MAX_NAME], int max) {
    uint32_t i, count = 0;
    if (max <= 0) return 0;
    for (i = 0; i < RAMFS_MAX_FILES && count < (uint32_t)max; i++) {
        if (!files[i].in_use) continue;
        uint32_t j;
        for (j = 0; j < RAMFS_MAX_NAME; j++)
            names[count][j] = files[i].name[j];
        count++;
    }
    return (int)count;
}

void ramfs_report(void) {
    uart_puts("=== RAMFS Report ===");
    uart_puts_nolf("  Files: ");
    uart_dec(file_count);
    uart_puts_nolf("/");
    uart_dec(RAMFS_MAX_FILES);
    uart_puts_nolf("  Total size: ");
    uint32_t total = 0, i;
    for (i = 0; i < RAMFS_MAX_FILES; i++)
        if (files[i].in_use) total += files[i].size;
    uart_dec(total);
    uart_puts(" bytes");
}
