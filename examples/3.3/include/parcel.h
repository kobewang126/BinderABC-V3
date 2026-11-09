// SPDX-License-Identifier: GPL-2.0-only
#ifndef PARCEL_H
#define PARCEL_H

#include <stddef.h>
#include <stdint.h>

struct parcel {
    unsigned char *data;
    size_t capacity;
    size_t size;
    size_t position;
};

void parcel_init(struct parcel *p, void *buffer, size_t capacity);
void parcel_rewind(struct parcel *p);
int parcel_write_u32(struct parcel *p, uint32_t value);
int parcel_read_u32(struct parcel *p, uint32_t *value);
int parcel_write_string(struct parcel *p, const char *text, size_t length);
int parcel_read_string(struct parcel *p, const unsigned char **text, size_t *length);

#endif
