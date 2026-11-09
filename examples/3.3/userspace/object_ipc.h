/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OBJECT_IPC_H
#define OBJECT_IPC_H
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <mini_binder.h>
#include <parcel.h>
#include "command.h"

static int object_transact(int fd, void *mapping, size_t page_size,
                           __u32 object, __u32 code, int32_t a, int32_t b, int32_t *result)
{
    struct mini_binder_transaction_data tx = { .code = code, .target_node = object };
    struct mini_binder_command request = { .cmd = MINI_BC_TRANSACTION };
    struct mini_binder_command reply = {0};
    struct mini_binder_command release = { .cmd = MINI_BC_FREE_REPLY };
    struct parcel p;
    uint32_t bits_a, bits_b, bits_result;
    int status;

    memcpy(&bits_a, &a, sizeof(a));
    memcpy(&bits_b, &b, sizeof(b));
    parcel_init(&p, tx.data, sizeof(tx.data));
    if (parcel_write_u32(&p, bits_a) || parcel_write_u32(&p, bits_b)) return -EINVAL;
    tx.size = (__u32)p.size;
    request.transaction = tx;
    status = exchange_command(fd, &request, &reply);
    if (status) return status;
    if (reply.cmd != MINI_BR_REPLY) return -EPROTO;
    tx = reply.transaction;
    status = tx.status;
    if (!status) {
        if (tx.size > page_size) status = -EPROTO;
        else {
            parcel_init(&p, mapping, page_size);
            p.size = tx.size;
            if (parcel_read_u32(&p, &bits_result) || p.position != p.size) status = -EPROTO;
            else memcpy(result, &bits_result, sizeof(*result));
        }
    }
    { int ret = exchange_command(fd, &release, NULL); if (ret && !status) status = ret; }
    return status;
}
#endif
