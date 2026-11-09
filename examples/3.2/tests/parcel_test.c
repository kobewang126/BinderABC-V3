// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <parcel.h>

int main(void)
{
    unsigned char bytes[16] = {0};
    struct parcel p;
    const unsigned char *text;
    size_t length;
    uint32_t value;

    parcel_init(&p, bytes, sizeof(bytes));
    assert(!parcel_write_u32(&p, 0x12345678));
    assert(bytes[0] == 0x78 && bytes[3] == 0x12);
    assert(!parcel_write_string(&p, "abc", 3));
    assert(p.size == 12 && bytes[11] == 0);
    assert(parcel_write_string(&p, "x", 1) == -1 && p.size == 12);
    parcel_rewind(&p);
    assert(!parcel_read_u32(&p, &value) && value == 0x12345678);
    assert(!parcel_read_string(&p, &text, &length));
    assert(length == 3 && !memcmp(text, "abc", 3));
    assert(parcel_read_u32(&p, &value) == -1 && p.position == 12);
    parcel_rewind(&p);
    p.size = 11;
    assert(!parcel_read_u32(&p, &value));
    assert(parcel_read_string(&p, &text, &length) == -1 && p.position == 4);
    parcel_init(&p, bytes, sizeof(bytes));
    assert(!parcel_write_string(&p, "", 0));
    parcel_rewind(&p);
    assert(!parcel_read_string(&p, &text, &length) && length == 0);
    memset(bytes, 0xff, 4);
    p.size = 4;
    parcel_rewind(&p);
    assert(parcel_read_string(&p, &text, &length) == -1 && p.position == 0);
    puts("parcel tests passed");
    return 0;
}
