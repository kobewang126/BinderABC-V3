// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBJECT_PARCEL_H
#define OBJECT_PARCEL_H

#include <stddef.h>
#include <stdint.h>
#include <parcel.h>

#define OBJECT_PARCEL_MAX_OBJECTS 8U
#define OBJECT_PARCEL_LOCAL 1U
#define OBJECT_PARCEL_HANDLE 2U
#define OBJECT_PARCEL_OBJECT_SIZE 12U

struct object_parcel {
    struct parcel bytes;
    uint32_t offsets[OBJECT_PARCEL_MAX_OBJECTS];
    size_t count;
};

struct parcel_object {
    uint32_t type;
    uint64_t value;
};

void object_parcel_init(struct object_parcel *p, void *buffer, size_t capacity);
int object_parcel_write(struct object_parcel *p, uint32_t type, uint64_t value);
int object_parcel_read(const struct object_parcel *p, size_t index, struct parcel_object *object);

int object_parcel_read_next(struct object_parcel *p, struct parcel_object *object);

#endif
