/*
 * AEOS - IPC (Inter-Process Communication)
 * Simple message queues for task-to-task communication
 */
#ifndef IPC_H
#define IPC_H

#include <stdint.h>

#define IPC_MAX_QUEUES  8
#define IPC_MAX_MSG     16
#define IPC_MSG_SIZE    64
#define IPC_OK          0
#define IPC_ERR_FULL   -1
#define IPC_ERR_EMPTY  -2
#define IPC_ERR_INVALID -3

typedef struct {
    uint32_t data[IPC_MSG_SIZE / 4];
    uint32_t len;
    uint32_t sender;
} ipc_msg_t;

typedef struct {
    uint32_t    id;
    uint32_t    owner;
    uint32_t    reader_count;
    uint8_t     in_use;
    ipc_msg_t   queue[IPC_MAX_MSG];
    uint32_t    head;
    uint32_t    tail;
    uint32_t    count;
} ipc_queue_t;

void     ipc_init(void);
int      ipc_create(uint32_t owner);
int      ipc_send(int queue_id, const void *data, uint32_t len, uint32_t sender);
int      ipc_recv(int queue_id, void *buf, uint32_t *len, uint32_t *sender);
int      ipc_destroy(int queue_id);
int      ipc_queue_count(void);
void     ipc_report(void);

#endif /* IPC_H */
