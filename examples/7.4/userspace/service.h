/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BINDERABC_SERVICE_H
#define BINDERABC_SERVICE_H
#include "lab.h"
#include <object_parcel.h>

_Static_assert(OBJECT_PARCEL_MAX_OBJECTS == MINI_BINDER_MAX_OBJECTS &&
               OBJECT_PARCEL_LOCAL == MINI_BINDER_OBJECT_LOCAL &&
               OBJECT_PARCEL_HANDLE == MINI_BINDER_OBJECT_HANDLE &&
               OBJECT_PARCEL_FD == MINI_BINDER_OBJECT_FD, "object protocol mismatch");

enum service_manager_code { SM_ADD_SERVICE = 1, SM_GET_SERVICE = 2 };
enum calculator_code { CALCULATOR_ADD = 1, CALCULATOR_SUBTRACT = 2 };
#define CALCULATOR_COOKIE 123U
#define CALCULATOR_NAME "calculator"

/* These bindings borrow received data; do not write to a mapped input Parcel. */
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
static inline int read_service_name(struct object_parcel *p, char name[33])
{
    const unsigned char *text; size_t size;
    if (parcel_read_string(&p->bytes, &text, &size) || !size || size > 32 || memchr(text, '\0', size))
        return -EINVAL;
    memcpy(name, text, size); name[size] = '\0'; return 0;
}
static inline int open_binder(unsigned char **mapping)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    if (fd < 0) { perror("open /dev/mini_binder"); exit(1); }
    *mapping = map_buffer(fd); return fd;
}
#endif
