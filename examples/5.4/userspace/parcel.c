// SPDX-License-Identifier: GPL-2.0-only
#include <stdint.h>
#include <string.h>
#include <parcel.h>

void parcel_init(struct parcel *p, void *buffer, size_t capacity)
{
    *p = (struct parcel){ .data = buffer, .capacity = capacity };
}

void parcel_rewind(struct parcel *p)
{
    p->position = 0;
}

int parcel_write_u32(struct parcel *p, uint32_t value)
{
    if (p->size > p->capacity || p->capacity - p->size < 4)
        return -1;
    for (unsigned int i = 0; i < 4; ++i)
        p->data[p->size + i] = (unsigned char)(value >> (i * 8));
    p->size += 4;
    return 0;
}

int parcel_read_u32(struct parcel *p, uint32_t *value)
{
    uint32_t result = 0;
    if (p->position > p->size || p->size - p->position < 4)
        return -1;
    for (unsigned int i = 0; i < 4; ++i)
        result |= (uint32_t)p->data[p->position + i] << (i * 8);
    p->position += 4;
    *value = result;
    return 0;
}

int parcel_write_string(struct parcel *p, const char *text, size_t length)
{
    size_t padding = (4 - length % 4) % 4;
    size_t available;

    if (length > UINT32_MAX || p->size > p->capacity)
        return -1;
    available = p->capacity - p->size;
    if (available < 4 || length > available - 4 || padding > available - 4 - length)
        return -1;
    parcel_write_u32(p, (uint32_t)length);
    if (length)
        memcpy(p->data + p->size, text, length);
    p->size += length;
    memset(p->data + p->size, 0, padding);
    p->size += padding;
    return 0;
}

int parcel_read_string(struct parcel *p, const unsigned char **text, size_t *length)
{
    size_t start = p->position;
    size_t padding;
    uint32_t n;

    if (parcel_read_u32(p, &n))
        return -1;
    padding = (4 - n % 4) % 4;
    if (n > p->size - p->position || padding > p->size - p->position - n) {
        p->position = start;
        return -1;
    }
    *text = p->data + p->position;
    *length = n;
    p->position += n + padding;
    return 0;
}
