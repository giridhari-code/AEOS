/*
 * AEOS - Device Framework
 * Unified device management: open/read/write/ioctl
 */
#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>

#define DEV_NAME_LEN    16
#define DEV_MAX         16
#define DEV_TYPE_CHAR   0
#define DEV_TYPE_BLOCK  1

/* Device operations */
typedef struct device {
    char     name[DEV_NAME_LEN];
    uint8_t  type;           /* DEV_TYPE_CHAR or DEV_TYPE_BLOCK */
    uint8_t  id;
    uint8_t  open_count;
    uint8_t  flags;
    int  (*open)(struct device *dev);
    int  (*read)(struct device *dev, void *buf, uint32_t len);
    int  (*write)(struct device *dev, const void *buf, uint32_t len);
    int  (*ioctl)(struct device *dev, uint32_t cmd, uint32_t arg);
    void (*close)(struct device *dev);
} device_t;

/* Device manager API */
void     device_init(void);
int      device_register(const char *name, uint8_t type, device_t *dev);
device_t *device_open(const char *name);
int      device_read(device_t *dev, void *buf, uint32_t len);
int      device_write(device_t *dev, const void *buf, uint32_t len);
int      device_ioctl(device_t *dev, uint32_t cmd, uint32_t arg);
void     device_close(device_t *dev);
device_t *device_get_by_id(uint8_t id);
void     device_list(void);

#endif /* DEVICE_H */
