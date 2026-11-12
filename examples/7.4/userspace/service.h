/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BINDERABC_SERVICE_H
#define BINDERABC_SERVICE_H
#include "lab.h"

enum service_manager_code { SM_ADD_SERVICE = 1, SM_GET_SERVICE = 2 };
enum calculator_code { CALCULATOR_ADD = 1, CALCULATOR_SUBTRACT = 2 };
#define CALCULATOR_COOKIE 123U
#define CALCULATOR_NAME "calculator"

static inline unsigned int put_name(struct mini_binder_transaction_data *tx, const char *name)
{
    size_t size = strlen(name);
    assert(size && size <= 32);
    put32(tx->data, (uint32_t)size); memcpy(tx->data + 4, name, size);
    tx->size = (uint32_t)((4 + size + 3) & ~3U);
    return tx->size;
}
static inline int get_name(const struct mini_binder_transaction_data *tx,
                           const unsigned char *data, char name[33], unsigned int *end)
{
    uint32_t size = tx->size >= 4 ? get32(data) : 0;
    if (!size || size > 32) return -EINVAL;
    *end = (4 + size + 3) & ~3U;
    if (*end > tx->size || memchr(data + 4, '\0', size)) return -EINVAL;
    memcpy(name, data + 4, size); name[size] = '\0';
    return 0;
}
static inline int open_binder(unsigned char **mapping)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    if (fd < 0) { perror("open /dev/mini_binder"); exit(1); }
    *mapping = map_buffer(fd); return fd;
}
static inline void call_service(int fd, struct mini_binder_command *call,
                                struct mini_binder_command *event)
{
    /* This demo uses ordinary callers: no nested calls or callback dispatch here. */
    assert(command_io(fd, call, event) == 0);
    assert(event->cmd == MINI_BR_REPLY);
}
#endif
