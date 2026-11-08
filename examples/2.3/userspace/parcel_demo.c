// SPDX-License-Identifier: GPL-2.0-only
#include <inttypes.h>
#include <stdio.h>
#include <parcel.h>

int main(void)
{
    unsigned char buffer[64];
    struct parcel p;
    const unsigned char *text;
    size_t length;
    uint32_t number;

    parcel_init(&p, buffer, sizeof(buffer));
    if (parcel_write_u32(&p, 42) || parcel_write_string(&p, "Binder", 6))
        return 1;
    printf("encoded size: %zu bytes\n", p.size);
    parcel_rewind(&p);
    if (parcel_read_u32(&p, &number) || parcel_read_string(&p, &text, &length))
        return 1;
    printf("decoded: number=%" PRIu32 ", text=%.*s\n", number, (int)length, text);
    return 0;
}
