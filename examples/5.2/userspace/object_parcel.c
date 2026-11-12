// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <stdint.h>
#include <object_parcel.h>

void object_parcel_init(struct object_parcel *p, void *buffer, size_t capacity)
{
    parcel_init(&p->bytes, buffer, capacity);
    p->count = 0;
}

int object_parcel_write(struct object_parcel *p, uint32_t type, uint64_t value)
{
    size_t start = p->bytes.size;
    if ((type != OBJECT_PARCEL_LOCAL && type != OBJECT_PARCEL_HANDLE) ||
        (type != OBJECT_PARCEL_LOCAL && value > UINT32_MAX) ||
        p->count >= OBJECT_PARCEL_MAX_OBJECTS || start > UINT32_MAX || start % 4)
        return -EINVAL;
    if (parcel_write_u32(&p->bytes, type) || parcel_write_u32(&p->bytes, (uint32_t)value) ||
        parcel_write_u32(&p->bytes, (uint32_t)(value >> 32))) {
        p->bytes.size = start;
        return -ENOSPC;
    }
    p->offsets[p->count++] = (uint32_t)start;
    return 0;
}

int object_parcel_read(const struct object_parcel *p, size_t index, struct parcel_object *object)
{
    struct parcel reader = p->bytes;
    uint32_t type, low, high;
    size_t offset;
    if (!object || p->count > OBJECT_PARCEL_MAX_OBJECTS || index >= p->count ||
        reader.size > reader.capacity) return -EINVAL;
    offset = p->offsets[index];
    if (offset % 4 || offset > reader.size || reader.size - offset < 12 ||
        (index && (p->offsets[index - 1] > offset || offset - p->offsets[index - 1] < 12)))
        return -EINVAL;
    reader.position = offset;
    if (parcel_read_u32(&reader, &type) || parcel_read_u32(&reader, &low) ||
        parcel_read_u32(&reader, &high) ||
        (type != OBJECT_PARCEL_LOCAL && type != OBJECT_PARCEL_HANDLE)) return -EINVAL;
    if (type != OBJECT_PARCEL_LOCAL && high) return -EINVAL;
    object->type = type;
    object->value = (uint64_t)low | ((uint64_t)high << 32);
    return 0;
}

int object_parcel_read_next(struct object_parcel *p, struct parcel_object *object)
{
    if (p->count > OBJECT_PARCEL_MAX_OBJECTS) return -EINVAL;
    for (size_t i = 0; i < p->count; i++) {
        if (p->offsets[i] != p->bytes.position) continue;
        int ret = object_parcel_read(p, i, object);
        if (!ret) p->bytes.position += OBJECT_PARCEL_OBJECT_SIZE;
        return ret;
    }
    return -EINVAL;
}
