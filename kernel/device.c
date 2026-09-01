/*
 * AEOS - Device Framework Implementation
 */
#include "device.h"
#include "uart.h"

static device_t *dev_table[DEV_MAX];
static uint8_t dev_count = 0;

void device_init(void) {
    uint8_t i;
    for (i = 0; i < DEV_MAX; i++)
        dev_table[i] = 0;
    dev_count = 0;
    uart_puts("  Device framework initialized");
}

int device_register(const char *name, uint8_t type, device_t *dev) {
    uint8_t i;
    if (dev_count >= DEV_MAX) return -1;

    for (i = 0; i < DEV_NAME_LEN && name[i]; i++)
        dev->name[i] = name[i];
    for (; i < DEV_NAME_LEN; i++)
        dev->name[i] = 0;

    dev->type = type;
    dev->id = dev_count;
    dev->open_count = 0;
    dev->flags = 0;

    dev_table[dev_count] = dev;
    dev_count++;

    uart_puts("  Device registered: ");
    uart_puts(name);
    return 0;
}

device_t *device_open(const char *name) {
    uint8_t i;
    for (i = 0; i < dev_count; i++) {
        if (!dev_table[i]) continue;
        uint8_t j;
        for (j = 0; j < DEV_NAME_LEN; j++) {
            if (dev_table[i]->name[j] != name[j]) break;
        }
        if (j == DEV_NAME_LEN || dev_table[i]->name[j] == name[j]) {
            dev_table[i]->open_count++;
            if (dev_table[i]->open)
                dev_table[i]->open(dev_table[i]);
            return dev_table[i];
        }
    }
    return 0;
}

int device_read(device_t *dev, void *buf, uint32_t len) {
    if (!dev || !dev->read) return -1;
    return dev->read(dev, buf, len);
}

int device_write(device_t *dev, const void *buf, uint32_t len) {
    if (!dev || !dev->write) return -1;
    return dev->write(dev, buf, len);
}

int device_ioctl(device_t *dev, uint32_t cmd, uint32_t arg) {
    if (!dev || !dev->ioctl) return -1;
    return dev->ioctl(dev, cmd, arg);
}

void device_close(device_t *dev) {
    if (!dev) return;
    if (dev->open_count > 0) dev->open_count--;
    if (dev->open_count == 0 && dev->close)
        dev->close(dev);
}

device_t *device_get_by_id(uint8_t id) {
    if (id >= DEV_MAX) return 0;
    return dev_table[id];
}

void device_list(void) {
    uint8_t i;
    uart_puts("=== Device List ===");
    for (i = 0; i < dev_count; i++) {
        if (!dev_table[i]) continue;
        uart_puts_nolf("  [");
        uart_dec(i);
        uart_puts_nolf("] ");
        uart_puts_nolf(dev_table[i]->name);
        uart_puts_nolf(" type=");
        uart_puts_nolf(dev_table[i]->type == DEV_TYPE_CHAR ? "char" : "block");
        uart_puts_nolf(" open=");
        uart_dec(dev_table[i]->open_count);
        uart_puts("");
    }
}
