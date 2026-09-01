/*
 * AEOS - IPC Implementation (Message Queues)
 */
#include "ipc.h"
#include "uart.h"
#include "memory.h"

static ipc_queue_t queues[IPC_MAX_QUEUES];

void ipc_init(void) {
    uint32_t i;
    for (i = 0; i < IPC_MAX_QUEUES; i++) {
        queues[i].in_use = 0;
        queues[i].head = 0;
        queues[i].tail = 0;
        queues[i].count = 0;
    }
    uart_puts("  IPC initialized (8 queues, 16 msgs each)");
}

int ipc_create(uint32_t owner) {
    uint32_t i;
    for (i = 0; i < IPC_MAX_QUEUES; i++) {
        if (!queues[i].in_use) {
            queues[i].id = i;
            queues[i].owner = owner;
            queues[i].reader_count = 0;
            queues[i].in_use = 1;
            queues[i].head = 0;
            queues[i].tail = 0;
            queues[i].count = 0;
            return (int)i;
        }
    }
    return IPC_ERR_FULL;
}

int ipc_send(int queue_id, const void *data, uint32_t len, uint32_t sender) {
    if (queue_id < 0 || queue_id >= IPC_MAX_QUEUES) return IPC_ERR_INVALID;
    ipc_queue_t *q = &queues[queue_id];
    if (!q->in_use) return IPC_ERR_INVALID;
    if (q->count >= IPC_MAX_MSG) return IPC_ERR_FULL;
    if (!data && len > 0) return IPC_ERR_INVALID;

    const uint8_t *src = (const uint8_t *)data;
    uint32_t copy_len = (len > IPC_MSG_SIZE) ? IPC_MSG_SIZE : len;

    ipc_msg_t *msg = &q->queue[q->tail];
    msg->len = copy_len;
    msg->sender = sender;
    if (copy_len > 0 && src)
        memcpy(msg->data, src, copy_len);
    else
        msg->len = 0;

    q->tail = (q->tail + 1) % IPC_MAX_MSG;
    q->count++;
    return IPC_OK;
}

int ipc_recv(int queue_id, void *buf, uint32_t *len, uint32_t *sender) {
    if (queue_id < 0 || queue_id >= IPC_MAX_QUEUES) return IPC_ERR_INVALID;
    if (!buf) return IPC_ERR_INVALID;
    ipc_queue_t *q = &queues[queue_id];
    if (!q->in_use) return IPC_ERR_INVALID;
    if (q->count == 0) return IPC_ERR_EMPTY;

    ipc_msg_t *msg = &q->queue[q->head];
    uint8_t *dst = (uint8_t *)buf;

    /* Copy out the message data (caller validated buffer >= IPC_MSG_SIZE) */
    memcpy(dst, msg->data, msg->len);

    if (len) *len = msg->len;
    if (sender) *sender = msg->sender;

    q->head = (q->head + 1) % IPC_MAX_MSG;
    q->count--;
    return IPC_OK;
}

int ipc_destroy(int queue_id) {
    if (queue_id < 0 || queue_id >= IPC_MAX_QUEUES) return IPC_ERR_INVALID;
    if (!queues[queue_id].in_use) return IPC_ERR_INVALID;
    queues[queue_id].in_use = 0;
    return IPC_OK;
}

int ipc_queue_count(void) {
    int count = 0;
    uint32_t i;
    for (i = 0; i < IPC_MAX_QUEUES; i++)
        if (queues[i].in_use) count++;
    return count;
}

void ipc_report(void) {
    uart_puts("=== IPC Report ===");
    uart_puts_nolf("  Queues: ");
    uart_dec(ipc_queue_count());
    uart_puts_nolf("/");
    uart_dec(IPC_MAX_QUEUES);
    uart_puts("");
    uint32_t i;
    for (i = 0; i < IPC_MAX_QUEUES; i++) {
        if (!queues[i].in_use) continue;
        uart_puts_nolf("  [");
        uart_dec(i);
        uart_puts_nolf("] msgs=");
        uart_dec(queues[i].count);
        uart_puts_nolf(" owner=");
        uart_dec(queues[i].owner);
        uart_puts("");
    }
}
