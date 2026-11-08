/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef CALC_IPC_H
#define CALC_IPC_H
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <mini_binder.h>
#include <parcel.h>

static int calc_transact(int fd, void *mapping, size_t page_size,
                         __u32 code, int32_t a, int32_t b, int32_t *result)
{
    struct mini_binder_transaction_data tx = { .code = code };
    struct parcel p;
    uint32_t bits_a, bits_b, bits_result;
    int status;

    memcpy(&bits_a, &a, sizeof(a));
    memcpy(&bits_b, &b, sizeof(b));
    parcel_init(&p, tx.data, sizeof(tx.data));
    if (parcel_write_u32(&p, bits_a) || parcel_write_u32(&p, bits_b)) return -EINVAL;
    tx.size = (__u32)p.size;
    if (ioctl(fd, MINI_BINDER_TRANSACT_MAPPED, &tx) < 0) return -errno;
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
    if (ioctl(fd, MINI_BINDER_RELEASE_REPLY) < 0 && !status) status = -errno;
    return status;
}
#endif
