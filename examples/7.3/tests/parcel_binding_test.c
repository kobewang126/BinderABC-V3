// SPDX-License-Identifier: GPL-2.0-only
#include <parcel_binding.h>
#include <stdio.h>

int main(void)
{
    struct mini_binder_transaction_data tx = {0};
    struct object_parcel output, input;
    struct parcel_object object;
    const unsigned char *name;
    size_t length;
    object_parcel_init(&output, tx.data, sizeof(tx.data));
    assert(parcel_write_string(&output.bytes, "calculator", 10) == 0);
    assert(object_parcel_write(&output, OBJECT_PARCEL_LOCAL, 123) == 0);
    parcel_to_transaction(&tx, &output);
    assert(tx.size == 28 && tx.object_count == 1 && tx.object_offsets[0] == 16);
    assert(tx.data[0] == 10 && !memcmp(tx.data + 4, "calculator", 10));
    assert(tx.data[16] == MINI_BINDER_OBJECT_LOCAL && tx.data[20] == 123);
    assert(parcel_from_transaction(&input, &tx, tx.data) == 0);
    assert(parcel_read_string(&input.bytes, &name, &length) == 0);
    assert(length == 10 && !memcmp(name, "calculator", length));
    assert(object_parcel_read_next(&input, &object) == 0);
    assert(object.type == OBJECT_PARCEL_LOCAL && object.value == 123);
    assert(input.bytes.position == input.bytes.size);
    assert(object_parcel_read_next(&input, &object) == -EINVAL);
    tx.size--;
    assert(parcel_from_transaction(&input, &tx, tx.data) == -EINVAL);
    tx.size++; tx.object_offsets[0] = 17;
    assert(parcel_from_transaction(&input, &tx, tx.data) == -EINVAL);
    tx.object_offsets[0] = 16; tx.object_count = MINI_BINDER_MAX_OBJECTS + 1;
    assert(parcel_from_transaction(&input, &tx, tx.data) == -EINVAL);
    object_parcel_init(&output, tx.data, 8);
    assert(object_parcel_write(&output, OBJECT_PARCEL_LOCAL, 77) == -ENOSPC);
    assert(output.bytes.size == 0 && output.count == 0);
    object_parcel_init(&output, tx.data, sizeof(tx.data));
    assert(object_parcel_write(&output, OBJECT_PARCEL_HANDLE, UINT64_C(1) << 32) == -EINVAL);
    assert(object_parcel_write(&output, OBJECT_PARCEL_FD, 5) == 0);
    parcel_to_transaction(&tx, &output);
    assert(parcel_from_transaction(&input, &tx, tx.data) == 0);
    assert(object_parcel_read_next(&input, &object) == 0);
    assert(object.type == OBJECT_PARCEL_FD && object.value == 5);
    /* Codec only: no file is transferred by this test. */
    puts("Parcel wire compatibility, sequential objects and boundary checks passed");
    return 0;
}
