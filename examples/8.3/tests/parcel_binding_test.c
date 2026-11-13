// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "../userspace/service.h"

int main(void)
{
    struct mini_binder_transaction_data tx = {0};
    struct object_parcel output, input;
    struct parcel_object object;
    unsigned char buffer[MINI_BINDER_MAX_DATA];
    char name[33];
    object_parcel_init(&output, buffer, sizeof(buffer));
    assert(parcel_write_string(&output.bytes, CALCULATOR_NAME, strlen(CALCULATOR_NAME)) == 0);
    assert(object_parcel_write(&output, OBJECT_PARCEL_LOCAL, CALCULATOR_COOKIE) == 0);
    parcel_to_transaction(&tx, &output);
    /* Same wire bytes as the preceding hand-written 7.4 encoder. */
    assert(tx.size == 28 && tx.object_count == 1 && tx.object_offsets[0] == 16);
    assert(get32(tx.data) == 10 && !memcmp(tx.data + 4, CALCULATOR_NAME, 10));
    assert(get32(tx.data + 16) == MINI_BINDER_OBJECT_LOCAL && get32(tx.data + 20) == CALCULATOR_COOKIE);
    assert(parcel_from_transaction(&input, &tx, tx.data) == 0);
    assert(read_service_name(&input, name) == 0 && !strcmp(name, CALCULATOR_NAME));
    assert(object_parcel_read_next(&input, &object) == 0 && object.type == OBJECT_PARCEL_LOCAL && object.value == CALCULATOR_COOKIE);
    assert(input.bytes.position == input.bytes.size);
    size_t end = input.bytes.position;
    assert(object_parcel_read_next(&input, &object) == -EINVAL && input.bytes.position == end);
    puts("name + LOCAL Parcel matches existing wire format; sequential decoding passed");

    object_parcel_init(&output, buffer, sizeof(buffer));
    assert(object_parcel_write(&output, OBJECT_PARCEL_HANDLE, 3) == 0);
    assert(object_parcel_write(&output, OBJECT_PARCEL_FD, 5) == 0);
    parcel_to_transaction(&tx, &output);
    assert(parcel_from_transaction(&input, &tx, tx.data) == 0);
    assert(object_parcel_read_next(&input, &object) == 0 && object.type == OBJECT_PARCEL_HANDLE && object.value == 3);
    assert(object_parcel_read_next(&input, &object) == 0 && object.type == OBJECT_PARCEL_FD && object.value == 5);
    /* These are only codec values: this test does not open or close fd 5. */
    put32(tx.data + 20, 1);
    assert(parcel_from_transaction(&input, &tx, tx.data) == -EINVAL);
    put32(tx.data + 20, 0);
    tx.object_offsets[1] = 4;
    assert(parcel_from_transaction(&input, &tx, tx.data) == -EINVAL);
    tx.object_offsets[1] = 12; tx.size = 23;
    assert(parcel_from_transaction(&input, &tx, tx.data) == -EINVAL);
    tx.size = 24; tx.object_count = MINI_BINDER_MAX_OBJECTS + 1;
    assert(parcel_from_transaction(&input, &tx, tx.data) == -EINVAL);
    object_parcel_init(&output, buffer, 8);
    assert(object_parcel_write(&output, OBJECT_PARCEL_LOCAL, 77) == -ENOSPC && output.bytes.size == 0 && output.count == 0);
    object_parcel_init(&output, buffer, sizeof(buffer));
    assert(object_parcel_write(&output, OBJECT_PARCEL_FD, UINT64_C(1) << 32) == -EINVAL);
    assert(object_parcel_write(&output, OBJECT_PARCEL_HANDLE, UINT64_C(1) << 32) == -EINVAL);
    puts("HANDLE/FD codec, malformed offsets, truncated objects and failed-write rollback passed");
    return 0;
}
