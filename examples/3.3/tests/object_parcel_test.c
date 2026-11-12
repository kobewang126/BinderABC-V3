// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <object_parcel.h>

int main(void)
{
    unsigned char data[64] = {0};
    unsigned char small[8] = {0};
    struct object_parcel p, short_p;
    struct parcel_object object;

    object_parcel_init(&p, data, sizeof(data));
    assert(parcel_write_u32(&p.bytes, 42) == 0);
    assert(object_parcel_write(&p, OBJECT_PARCEL_LOCAL, UINT64_C(0x1122334455667788)) == 0);
    assert(parcel_write_u32(&p.bytes, 7) == 0);
    assert(object_parcel_write(&p, OBJECT_PARCEL_HANDLE, 9) == 0);
    assert(p.count == 2 && p.offsets[0] == 4 && p.offsets[1] == 20);
    assert(object_parcel_read(&p, 0, &object) == 0 && object.type == OBJECT_PARCEL_LOCAL &&
           object.value == UINT64_C(0x1122334455667788));
    assert(object_parcel_read(&p, 1, &object) == 0 && object.type == OBJECT_PARCEL_HANDLE && object.value == 9);
    printf("Parcel: %zu bytes, %zu objects\n", p.bytes.size, (size_t)p.count);
    for (unsigned int i = 0; i < p.count; i++) {
        assert(object_parcel_read(&p, i, &object) == 0);
        printf("offsets[%u]=%u -> %s, value=0x%llx\n", i, p.offsets[i],
               object.type == OBJECT_PARCEL_LOCAL ? "LOCAL" : "HANDLE",
               (unsigned long long)object.value);
    }
    puts("ordinary integers at offsets 0 and 16 are not object entries");
    puts("regression: reject overlap, out-of-range offsets, invalid type and insufficient space");
    p.offsets[1] = 8;
    assert(object_parcel_read(&p, 1, &object) == -EINVAL);
    p.offsets[1] = 60;
    assert(object_parcel_read(&p, 1, &object) == -EINVAL);
    p.offsets[1] = 20;
    data[20] = 99;
    assert(object_parcel_read(&p, 1, &object) == -EINVAL);
    object_parcel_init(&short_p, small, sizeof(small));
    assert(object_parcel_write(&short_p, OBJECT_PARCEL_LOCAL, 1) == -ENOSPC);
    assert(short_p.bytes.size == 0 && short_p.count == 0);
    puts("object offsets, decoding, malformed data and rollback passed");
    return 0;
}
