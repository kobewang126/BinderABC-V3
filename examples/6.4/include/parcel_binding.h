/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BINDERABC_PARCEL_BINDING_H
#define BINDERABC_PARCEL_BINDING_H
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <mini_binder.h>
#include <object_parcel.h>

/* Borrowed mapped input: read only, valid until reply submission / FREE_REPLY. */
static inline int parcel_from_transaction(struct object_parcel *p,
                                          const struct mini_binder_transaction_data *tx,
                                          const void *data)
{
    if (tx->size > MINI_BINDER_MAX_DATA || tx->object_count > OBJECT_PARCEL_MAX_OBJECTS)
        return -EINVAL;
    object_parcel_init(p, (void *)data, tx->size);
    p->bytes.size = tx->size; p->count = tx->object_count;
    memcpy(p->offsets, tx->object_offsets, p->count * sizeof(p->offsets[0]));
    for (size_t i = 0; i < p->count; i++) {
        struct parcel_object object;
        if (object_parcel_read(p, i, &object)) return -EINVAL;
    }
    return 0;
}
static inline void parcel_to_transaction(struct mini_binder_transaction_data *tx,
                                         const struct object_parcel *p)
{
    assert(p->bytes.size <= MINI_BINDER_MAX_DATA && p->count <= MINI_BINDER_MAX_OBJECTS);
    tx->size = (uint32_t)p->bytes.size; tx->object_count = (uint32_t)p->count;
    memset(tx->object_offsets, 0, sizeof(tx->object_offsets));
    memcpy(tx->object_offsets, p->offsets, p->count * sizeof(p->offsets[0]));
    if (tx->data != p->bytes.data) memcpy(tx->data, p->bytes.data, p->bytes.size);
}
#endif
